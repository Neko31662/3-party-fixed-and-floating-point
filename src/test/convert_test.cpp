#include "protocol/convert.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int ell = 64;
const ShareValue p2 = 3 + (ShareValue(1) << 28);
const int test_nums = 1000;
const int vec_len = 10;

int main(int argc, char **argv) {
    // net io
    int party_id = atoi(argv[1]);
    NetIOMP *netio = new NetIOMP(party_id, BASE_PORT, NUM_PARTIES);
    netio->sync();

    // setup prg
    block seed1;
    block seed2;
    if (party_id == 0) {
        seed1 = gen_seed();
        seed2 = gen_seed();
        netio->send_data(1, &seed1, sizeof(block));
        netio->send_data(2, &seed2, sizeof(block));
    } else if (party_id == 1) {
        netio->recv_data(0, &seed1, sizeof(block));
        seed2 = gen_seed();
        netio->send_data(2, &seed2, sizeof(block));
    } else if (party_id == 2) {
        netio->recv_data(0, &seed1, sizeof(block));
        netio->recv_data(1, &seed2, sizeof(block));
    }
    vector<PRGSync> PRGs = {PRGSync(&seed1), PRGSync(&seed2)};
    auto private_seed = gen_seed();
    auto private_PRG = PRGSync(&private_seed);

    for (int test_i = 0; test_i < test_nums; test_i++) {
        PI_convert_intermediate intermediate(p2);
        ADDshare<> x_share(ell);
        MSSshare_p z_share{p2};

        ShareValue x_plain = 0;
        private_PRG.gen_random_data(&x_plain, sizeof(ShareValue));
        x_plain &= x_share.MASK;
        x_plain >>= 1; // 保证最高位是0

        // preprocess
        PI_convert_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &z_share);
        if (party_id == 0) {
            netio->send_stored_data(1);
            netio->send_stored_data(2);
        }
        if (party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ADDshare_share_from(0, party_id, PRGs, *netio, &x_share, x_plain);
        ShareValue x_recon = ADDshare_recon(party_id, *netio, &x_share);

        // compute
        PI_convert(party_id, PRGs, *netio, intermediate, &x_share, &z_share);

        // reconstruct
        ShareValue z_recon = MSSshare_p_recon(party_id, *netio, &z_share);

        // verify
        if (party_id == 0) {
            ShareValue expected = x_plain % p2;
            if (z_recon != expected) {
                cout << "PI_convert test failed! expected: " << expected << ", got: " << z_recon
                     << ", at test " << test_i << endl;
                exit(1);
            }
        }
    }
    cout << "PI_convert test passed!" << std::endl;

    for (int test_i = 0; test_i < max(1, test_nums / vec_len); test_i++) {
        vector<PI_convert_intermediate> intermediate_vec(vec_len, p2);
        vector<ADDshare<>> x_share_vec(vec_len, ell);
        vector<MSSshare_p> z_share_vec(vec_len, p2);

        ShareValue x_plain = 0;
        private_PRG.gen_random_data(&x_plain, sizeof(ShareValue));
        x_plain &= x_share_vec[0].MASK;
        x_plain >>= 1; // 保证最高位是0

        // preprocess
        for (int i = 0; i < vec_len; i++) {
            PI_convert_preprocess(party_id, PRGs, *netio, intermediate_vec[i], &x_share_vec[i],
                                  &z_share_vec[i]);
        }
        if (party_id == 0) {
            netio->send_stored_data(1);
            netio->send_stored_data(2);
        }
        if (party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        for (int i = 0; i < vec_len; i++) {
            ADDshare_share_from(0, party_id, PRGs, *netio, &x_share_vec[i], x_plain);
        }
        ShareValue x_recon = ADDshare_recon(party_id, *netio, &x_share_vec[0]);

        // compute
        auto intermediate_ptr = make_ptr_vec(intermediate_vec);
        auto x_share_ptr = make_ptr_vec(x_share_vec);
        auto z_share_ptr = make_ptr_vec(z_share_vec);
        PI_convert_vec(party_id, PRGs, *netio, intermediate_ptr, x_share_ptr, z_share_ptr);

        // reconstruct
        for (int i = 0; i < vec_len; i++) {
            ShareValue z_recon = MSSshare_p_recon(party_id, *netio, &z_share_vec[i]);

            // verify
            if (party_id == 0) {
                ShareValue expected = x_plain % p2;
                if (z_recon != expected) {
                    cout << "PI_convert test failed! expected: " << expected << ", got: " << z_recon
                         << ", at test " << test_i << endl;
                    exit(1);
                }
            }
        }
    }
    cout << "PI_convert_vec test passed!" << std::endl;

    delete netio;
}