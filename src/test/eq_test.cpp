#include "protocol/eq.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 64;
const ShareValue k = 67676767;
const int vec_len = 10;

using namespace std;

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
        PI_eq_intermediate intermediate(ell, k);
        MSSshare x_share(ell), y_share(ell);
        MSSshare_p b_share{k};

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(1, party_id, PRGs, *netio, &y_share);
        PI_eq_preprocess(party_id, PRGs, *netio, private_PRG, intermediate, &x_share, &y_share,
                         &b_share);
        if (party_id == 0) {
            netio->send_stored_data(1);
            netio->send_stored_data(2);
        }
        if (party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue test_value_x = 0;
        ShareValue test_value_y = 0;
        if (party_id == 0) {
            private_PRG.gen_random_data(&test_value_x, sizeof(ShareValue));
            test_value_x &= x_share.MASK;
            private_PRG.gen_random_data(&test_value_y, sizeof(ShareValue));
            test_value_y &= x_share.MASK;
            if (test_i % 2 == 0) {
                test_value_y = test_value_x;
            }
        }
        MSSshare_share_from(0, party_id, *netio, &x_share, test_value_x);
        MSSshare_share_from(0, party_id, *netio, &y_share, test_value_y);

        // compute
        PI_eq(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &b_share);

        // reveal
        ShareValue rec = MSSshare_p_recon(party_id, *netio, &b_share);
        ShareValue expected = (test_value_x == test_value_y) ? 1 : 0;
        if (rec != expected && party_id == 0) {
            cout << "Test " << test_i << " failed! Expected: " << expected << ", got: " << rec
                 << endl;
            cout << "x: " << bitset<ell>(test_value_x) << endl;
            cout << "y: " << bitset<ell>(test_value_y) << endl;
            exit(1);
        }
    }
    cout << "PI_eq test passed!" << endl;

    for (int test_i = 0; test_i < max(1, test_nums / vec_len); test_i++) {
        vector<PI_eq_intermediate> intermediate_vec(vec_len, PI_eq_intermediate(ell, k));
        vector<MSSshare> x_share_vec(vec_len, ell), y_share_vec(vec_len, ell);
        vector<MSSshare_p> b_share_vec(vec_len, k);

        // preprocess
        for (int i = 0; i < vec_len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share_vec[i]);
            MSSshare_preprocess(1, party_id, PRGs, *netio, &y_share_vec[i]);
            PI_eq_preprocess(party_id, PRGs, *netio, private_PRG, intermediate_vec[i],
                             &x_share_vec[i], &y_share_vec[i], &b_share_vec[i]);
        }
        if (party_id == 0) {
            netio->send_stored_data(1);
            netio->send_stored_data(2);
        }
        if (party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue test_value_x = 0;
        ShareValue test_value_y = 0;
        if (party_id == 0) {
            private_PRG.gen_random_data(&test_value_x, sizeof(ShareValue));
            test_value_x &= x_share_vec[0].MASK;
            private_PRG.gen_random_data(&test_value_y, sizeof(ShareValue));
            test_value_y &= x_share_vec[0].MASK;
            if (test_i % 2 == 0) {
                test_value_y = test_value_x;
            }
        }
        for (int i = 0; i < vec_len; i++) {
            MSSshare_share_from(0, party_id, *netio, &x_share_vec[i], test_value_x);
            MSSshare_share_from(0, party_id, *netio, &y_share_vec[i], test_value_y);
        }

        // compute
        auto intermediate_ptr = make_ptr_vec(intermediate_vec);
        auto x_share_ptr = make_ptr_vec(x_share_vec);
        auto y_share_ptr = make_ptr_vec(y_share_vec);
        auto b_share_ptr = make_ptr_vec(b_share_vec);
        PI_eq_vec(party_id, PRGs, *netio, intermediate_ptr, x_share_ptr, y_share_ptr, b_share_ptr);

        // reveal
        for (int i = 0; i < vec_len; i++) {
            MSSshare_p &b_share = b_share_vec[i];
            ShareValue rec = MSSshare_p_recon(party_id, *netio, &b_share);
            ShareValue expected = (test_value_x == test_value_y) ? 1 : 0;
            if (rec != expected && party_id == 0) {
                cout << "Test " << test_i << " failed! Expected: " << expected << ", got: " << rec
                     << endl;
                cout << "x: " << bitset<ell>(test_value_x) << endl;
                cout << "y: " << bitset<ell>(test_value_y) << endl;
                exit(1);
            }
        }
    }
    cout << "PI_eq_vec test passed!" << endl;
    delete netio;
    return 0;
}