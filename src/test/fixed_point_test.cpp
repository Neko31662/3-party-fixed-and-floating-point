#include "protocol/fixed_point.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 10000;
const int li = 4;
const int lf = 4;
const int l_res = li + lf + 1;

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
        MSSshare<li + lf> x_share, y_share;
        MSSshare<l_res> z_share;
        PI_fixed_mult_intermediate<li, lf, l_res> intermediate;

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        PI_fixed_mult_preprocess<li, lf, l_res>(party_id, PRGs, *netio, intermediate, &x_share,
                                                &y_share, &z_share);

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
        PI_fixed_mult<li, lf, l_res>(party_id, PRGs, *netio, intermediate, &x_share, &y_share,
                                     &z_share);

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

        ShareValue z_plain = (LongShareValue)x_recon * y_recon;
        bool z_wrap = (z_plain & (ShareValue(1) << (lf - 1))) ? 1 : 0;
        z_plain >>= lf;
        z_plain += z_wrap;
        z_plain &= z_share.MASK;
        ShareValue dif = z_plain > z_recon ? z_plain - z_recon : z_recon - z_plain;
        if (dif > 1) {
            // 排除特殊情况：00000000 和 11111111
            ShareValue mx = max(z_plain, z_recon), mn = min(z_plain, z_recon);
            if (!(mn == 0 && mx == z_share.MASK)) {
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
    delete netio;
}