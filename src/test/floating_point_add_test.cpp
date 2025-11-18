#include "protocol/floating_point_add.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1;
const int lf = 15;
const int le = 6;

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
    block public_seed;
    if (party_id == 0) {
        public_seed = gen_seed();
        netio->send_data(1, &public_seed, sizeof(block));
        netio->send_data(2, &public_seed, sizeof(block));
    } else {
        netio->recv_data(0, &public_seed, sizeof(block));
    }
    PRGSync public_PRG(&public_seed);

    for (int test_i = 0; test_i < test_nums; test_i++) {
        FLTshare x_share(lf, le);
        FLTshare y_share(lf, le);
        FLTshare z_share(lf, le);
        PI_float_add_intermediate intermediate(lf, le);

        // preprocess
        FLTshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        FLTshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        PI_float_add_preprocess(party_id, PRGs, *netio, private_PRG, intermediate, &x_share,
                                &y_share, &z_share);

        if (party_id == 0) {
            netio->send_stored_data(1);
            netio->send_stored_data(2);
        }
        if (party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue x_b, x_t, x_e;
        ShareValue y_b, y_t, y_e;
        if (party_id == 0) {
            private_PRG.gen_random_data(&x_b, sizeof(ShareValue));
            private_PRG.gen_random_data(&x_t, sizeof(ShareValue));
            private_PRG.gen_random_data(&x_e, sizeof(ShareValue));
            x_b &= 1;
            x_t &= (ShareValue(1) << (lf + 1)) - 1;
            x_e &= (ShareValue(1) << (le - 2)) - 1;
            private_PRG.gen_random_data(&y_b, sizeof(ShareValue));
            private_PRG.gen_random_data(&y_t, sizeof(ShareValue));
            private_PRG.gen_random_data(&y_e, sizeof(ShareValue));
            y_b &= 1;
            y_t &= (ShareValue(1) << (lf + 1)) - 1;
            y_e &= (ShareValue(1) << (le - 2)) - 1;

            x_t |= (ShareValue(1) << lf);
            y_t |= (ShareValue(1) << lf);
            x_e += (ShareValue(1) << (le - 2));
            y_e += (ShareValue(1) << (le - 2));
        }
        FLTshare_share_from(0, party_id, *netio, &x_share, x_b, x_t, x_e);
        FLTshare_share_from(0, party_id, *netio, &y_share, y_b, y_t, y_e);

        // compute
        PI_float_add(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &z_share);

        // reveal
        FLTplain x_recon = FLTshare_recon(party_id, *netio, &x_share);
        FLTplain y_recon = FLTshare_recon(party_id, *netio, &y_share);
        FLTplain z_recon = FLTshare_recon(party_id, *netio, &z_share);

        // verify
        if (party_id == 0) {
            uint256_t x_value = x_t;
            {
                int tmp = x_e;
                while (tmp > 0) {
                    x_value = x_value << 1;
                    tmp--;
                }
            }
            uint256_t y_value = y_t;
            {
                int tmp = y_e;
                while (tmp > 0) {
                    y_value = y_value << 1;
                    tmp--;
                }
            }
            bool x_sign = x_b, y_sign = y_b;
            if (x_value < y_value) {
                swap(x_value, y_value);
                swap(x_sign, y_sign);
            }
            if (x_sign == y_sign) {
                x_value = x_value + y_value;
            } else {
                x_value = x_value - y_value;
            }
            bool z_b = x_sign;
            uint256_t z_t = x_value;
            int z_e = 0;
            while (z_t >= (uint256_t(1) << (lf + 1))) {
                z_t = z_t >> 1;
                z_e++;
            }

            cout << endl << "Info of x:" << endl;
            cout << "b        :" << x_b << endl;
            cout << "t        :" << bitset<lf + 2>(x_t) << endl;
            cout << "e        :" << bitset<le>(x_e) << endl;

            cout << endl << "Reconstructed x:" << endl;
            cout << "b        :" << x_recon.b << endl;
            cout << "t        :" << bitset<lf + 2>(x_recon.t) << endl;
            cout << "e        :" << bitset<le>(x_recon.e) << endl;

            cout << endl << "Info of y:" << endl;
            cout << "b        :" << y_b << endl;
            cout << "t        :" << bitset<lf + 2>(y_t) << endl;
            cout << "e        :" << bitset<le>(y_e) << endl;

            cout << endl << "Reconstructed y:" << endl;
            cout << "b        :" << y_recon.b << endl;
            cout << "t        :" << bitset<lf + 2>(y_recon.t) << endl;
            cout << "e        :" << bitset<le>(y_recon.e) << endl;

            cout << endl << "Info of z:" << endl;
            cout << "b        :" << z_b << endl;
            cout << "t        :" << bitset<lf + 2>(z_t.convert_to<ShareValue>()) << endl;
            cout << "e        :" << bitset<le>(z_e) << endl;

            cout << endl << "Reconstructed z:" << endl;
            cout << "b        :" << z_recon.b << endl;
            cout << "t        :" << bitset<lf + 2>(z_recon.t) << endl;
            cout << "e        :" << bitset<le>(z_recon.e) << endl;
        }
    }
}