#include "protocol/fixed_point.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 10000;
const int li = 23;
const int lf = 8;
const int l_res = li + lf;
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
        MSSshare x_share(li + lf), y_share(li + lf);
        MSSshare z_share(l_res);
        PI_fixed_mult_intermediate intermediate(li, lf, l_res);

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        PI_fixed_mult_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share,
                                 &z_share);

        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue x_plain, y_plain;
        if (party_id == 0) {
            private_PRG.gen_random_data(&x_plain, sizeof(ShareValue));
            private_PRG.gen_random_data(&y_plain, sizeof(ShareValue));
            x_plain &= (x_share.MASK >> 1);
            x_plain -= (ShareValue(1) << (li + lf - 2));
            y_plain &= (y_share.MASK >> 1);
            y_plain -= (ShareValue(1) << (li + lf - 2));
        }
        MSSshare_share_from(0, party_id, *netio, &x_share, x_plain);
        MSSshare_share_from(0, party_id, *netio, &y_share, y_plain);

        // compute
        PI_fixed_mult(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &z_share);

        // reconstruct
        ShareValue x_recon, y_recon, z_recon;
        x_recon = MSSshare_recon(party_id, *netio, &x_share);
        y_recon = MSSshare_recon(party_id, *netio, &y_share);

        x_recon += (ShareValue(1) << (li + lf - 2));
        x_recon &= x_share.MASK;
        x_recon -= (ShareValue(1) << (li + lf - 2));
        y_recon += (ShareValue(1) << (li + lf - 2));
        y_recon &= y_share.MASK;
        y_recon -= (ShareValue(1) << (li + lf - 2));

        z_recon = MSSshare_recon(party_id, *netio, &z_share);

        ShareValue z_plain = x_recon * y_recon;
        bool z_wrap = (z_plain & (ShareValue(1) << (lf - 1))) ? 1 : 0;
        z_plain >>= lf;
        z_plain += z_wrap;
        z_plain &= z_share.MASK;
        ShareValue dif = z_plain > z_recon ? z_plain - z_recon : z_recon - z_plain;
        if (dif > 1) {
            // 排除特殊情况：00000000 和 11111111
            ShareValue mx = max(z_plain, z_recon), mn = min(z_plain, z_recon);
            if (!(mn == 0 && mx == z_share.MASK)) {
                cout << "Test " << test_i << " failed!" << endl;
                cout << "x_recon: " << bitset<l_res>(x_recon) << endl;
                cout << "y_recon: " << bitset<l_res>(y_recon) << endl;
                cout << "z_recon: " << bitset<l_res>(z_recon) << endl;
                cout << "z_plain: " << bitset<l_res>(z_plain) << endl;
                cout << "Error: incorrect fixed-point multiplication at index " << test_i << endl;
                exit(1);
            }
        }
    }
    cout << "PI_fixed_mult test passed!" << endl;

    for (int test_i = 0; test_i < max(1, test_nums / vec_len); test_i++) {
        vector<MSSshare> x_share_vec;
        vector<MSSshare> y_share_vec;
        vector<MSSshare> z_share_vec;
        vector<PI_fixed_mult_intermediate> intermediate_vec;
        for (int vi = 0; vi < vec_len; vi++) {
            x_share_vec.emplace_back(li + lf);
            y_share_vec.emplace_back(li + lf);
            z_share_vec.emplace_back(l_res);
            intermediate_vec.emplace_back(li, lf, l_res);
        }

        // preprocess
        for (int vi = 0; vi < vec_len; vi++) {
            MSSshare &x_share = x_share_vec[vi];
            MSSshare &y_share = y_share_vec[vi];
            MSSshare &z_share = z_share_vec[vi];
            PI_fixed_mult_intermediate &intermediate = intermediate_vec[vi];
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
            PI_fixed_mult_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share,
                                     &z_share);
        }

        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        vector<ShareValue> x_plain_vec(vec_len), y_plain_vec(vec_len);
        for (int vi = 0; vi < vec_len; vi++) {
            if (party_id == 0) {
                private_PRG.gen_random_data(&x_plain_vec[vi], sizeof(ShareValue));
                private_PRG.gen_random_data(&y_plain_vec[vi], sizeof(ShareValue));
                x_plain_vec[vi] &= (x_share_vec[vi].MASK >> 1);
                x_plain_vec[vi] -= (ShareValue(1) << (li + lf - 2));
                y_plain_vec[vi] &= (y_share_vec[vi].MASK >> 1);
                y_plain_vec[vi] -= (ShareValue(1) << (li + lf - 2));
            }
            MSSshare_share_from(0, party_id, *netio, &x_share_vec[vi], x_plain_vec[vi]);
            MSSshare_share_from(0, party_id, *netio, &y_share_vec[vi], y_plain_vec[vi]);
        }

        // compute
        auto intermediate_ptr = make_ptr_vec(intermediate_vec);
        auto x_share_ptr = make_ptr_vec(x_share_vec);
        auto y_share_ptr = make_ptr_vec(y_share_vec);
        auto z_share_ptr = make_ptr_vec(z_share_vec);
        PI_fixed_mult_vec(party_id, PRGs, *netio, intermediate_ptr, x_share_ptr, y_share_ptr,
                          z_share_ptr);

        // reconstruct
        vector<ShareValue> x_recon_vec(vec_len), y_recon_vec(vec_len), z_recon_vec(vec_len);
        for (int vi = 0; vi < vec_len; vi++) {
            MSSshare &x_share = x_share_vec[vi];
            MSSshare &y_share = y_share_vec[vi];
            MSSshare &z_share = z_share_vec[vi];
            ShareValue &x_recon = x_recon_vec[vi];
            ShareValue &y_recon = y_recon_vec[vi];
            ShareValue &z_recon = z_recon_vec[vi];
            x_recon = MSSshare_recon(party_id, *netio, &x_share);
            y_recon = MSSshare_recon(party_id, *netio, &y_share);

            x_recon += (ShareValue(1) << (li + lf - 2));
            x_recon &= x_share.MASK;
            x_recon -= (ShareValue(1) << (li + lf - 2));
            y_recon += (ShareValue(1) << (li + lf - 2));
            y_recon &= y_share.MASK;
            y_recon -= (ShareValue(1) << (li + lf - 2));

            z_recon = MSSshare_recon(party_id, *netio, &z_share);

            ShareValue z_plain = x_recon * y_recon;
            bool z_wrap = (z_plain & (ShareValue(1) << (lf - 1))) ? 1 : 0;
            z_plain >>= lf;
            z_plain += z_wrap;
            z_plain &= z_share.MASK;
            ShareValue dif = z_plain > z_recon ? z_plain - z_recon : z_recon - z_plain;
            if (dif > 1) {
                // 排除特殊情况：00000000 和 11111111
                ShareValue mx = max(z_plain, z_recon), mn = min(z_plain, z_recon);
                if (!(mn == 0 && mx == z_share.MASK)) {
                    cout << "Failed at vector index " << vi << " of test " << test_i << endl;
                    cout << "x_recon: " << bitset<l_res>(x_recon) << endl;
                    cout << "y_recon: " << bitset<l_res>(y_recon) << endl;
                    cout << "z_recon: " << bitset<l_res>(z_recon) << endl;
                    cout << "z_plain: " << bitset<l_res>(z_plain) << endl;
                    cout << "Error: incorrect fixed-point multiplication at index " << test_i
                         << endl;
                    exit(1);
                }
            }
        }
    }
    cout << "PI_fixed_mult vector test passed!" << endl;
    delete netio;
}