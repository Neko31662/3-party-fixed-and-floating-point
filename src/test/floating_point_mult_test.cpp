#include "protocol/floating_point_mult.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 10000;
const int lf = 28;
const ShareValue le = 16;
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

    // 非向量
    for (int test_i = 0; test_i < test_nums; test_i++) {
        FLTshare x_share(lf, le), y_share(lf, le), z_share(lf, le);
        PI_float_mult_intermediate intermediate(lf, le);

        // preprocess
        FLTshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        FLTshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        PI_float_mult_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share,
                                 &z_share);

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
            x_t &= ShareValue(1) << (lf + 1) - 1;
            x_e &= ShareValue(1) << (le - 1) - 1;
            private_PRG.gen_random_data(&y_b, sizeof(ShareValue));
            private_PRG.gen_random_data(&y_t, sizeof(ShareValue));
            private_PRG.gen_random_data(&y_e, sizeof(ShareValue));
            y_b &= 1;
            y_t &= ShareValue(1) << (lf + 1) - 1;
            y_e &= ShareValue(1) << (le - 1) - 1;

            x_t |= (ShareValue(1) << lf);
            y_t |= (ShareValue(1) << lf);
            x_e += (ShareValue(1) << (le - 2));
            y_e += (ShareValue(1) << (le - 2));
        }
        FLTshare_share_from(0, party_id, *netio, &x_share, x_b, x_t, x_e);
        FLTshare_share_from(0, party_id, *netio, &y_share, y_b, y_t, y_e);

        // compute
        PI_float_mult(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &z_share);

        // reveal
        FLTplain x_recon = FLTshare_recon(party_id, *netio, &x_share);
        FLTplain y_recon = FLTshare_recon(party_id, *netio, &y_share);
        FLTplain z_recon = FLTshare_recon(party_id, *netio, &z_share);
        if (party_id == 0) {
            ShareValue z_t_plain = x_t * y_t;
            bool z_wrap = (z_t_plain & (ShareValue(1) << (lf - 1))) ? 1 : 0;
            z_t_plain >>= lf;
            z_t_plain += z_wrap;
            z_t_plain &= ShareValue((ShareValue(1) << (lf + 2)) - 1);

            ShareValue z_e_plain = x_e + y_e - (ShareValue(1) << (le - 1));
            if (z_t_plain & (ShareValue(1) << (lf + 1))) {
                z_wrap = (z_t_plain & 1) ? 1 : 0;
                z_t_plain >>= 1;
                z_e_plain += 1;
                z_t_plain += z_wrap;
            }
            z_e_plain &= ShareValue((ShareValue(1) << le) - 1);

            ShareValue z_b_plain = (x_b + y_b) % 2;

            bool flag[5] = {0};

            if (z_recon.b != z_b_plain) {
                flag[0] = 1;
            }

            ShareValue dif = z_t_plain > z_recon.t ? z_t_plain - z_recon.t : z_recon.t - z_t_plain;
            if (dif > 2) {
                flag[1] = 1;
            }

            if (z_recon.e != z_e_plain) {
                flag[2] = 1;
            }

            // 检测明文最高位1的位置是否正常
            if ((z_t_plain & (ShareValue(1) << (lf + 1))) || !(z_t_plain & (ShareValue(1) << lf))) {
                flag[3] = 1;
            }

            // 检测重构结果最高位1的位置是否正常
            if ((z_recon.t & (ShareValue(1) << (lf + 1))) || !(z_recon.t & (ShareValue(1) << lf))) {
                flag[4] = 1;
            }

            for (int i = 0; i < 5; i++) {
                if (flag[i]) {
                    cout << "Test " << test_i << " failed!" << endl;
                    cout << "Flag: ";
                    for (int j = 0; j < 5; j++) {
                        cout << flag[j] << " ";
                    }
                    cout << endl;
                    cout << "dif: " << dif << endl;

                    cout << endl << "Info of x:" << endl;
                    cout << "b        :" << x_b << endl;
                    cout << "t        :" << bitset<lf + 2>(x_t) << endl;
                    cout << "e        :" << bitset<le>(x_e) << endl;

                    cout << endl << "Info of y:" << endl;
                    cout << "b        :" << y_b << endl;
                    cout << "t        :" << bitset<lf + 2>(y_t) << endl;
                    cout << "e        :" << bitset<le>(y_e) << endl;

                    cout << endl << "Info of z:" << endl;
                    cout << "b        :" << z_b_plain << endl;
                    cout << "t        :" << bitset<lf + 2>(z_t_plain) << endl;
                    cout << "e        :" << bitset<le>(z_e_plain) << endl;

                    cout << endl << "Reconstructed x:" << endl;
                    cout << "b        :" << x_recon.b << endl;
                    cout << "t        :" << bitset<lf + 2>(x_recon.t) << endl;
                    cout << "e        :" << bitset<le>(x_recon.e) << endl;

                    cout << endl << "Reconstructed y:" << endl;
                    cout << "b        :" << y_recon.b << endl;
                    cout << "t        :" << bitset<lf + 2>(y_recon.t) << endl;
                    cout << "e        :" << bitset<le>(y_recon.e) << endl;

                    cout << endl << "Reconstructed z:" << endl;
                    cout << "b        :" << z_recon.b << endl;
                    cout << "t        :" << bitset<lf + 2>(z_recon.t) << endl;
                    cout << "e        :" << bitset<le>(z_recon.e) << endl;
                    exit(1);
                }
            }
        }
    }
    cout << "PI_float_mult test passed!" << endl;

    // 向量
    for (int test_i = 0; test_i < max(1, test_nums / vec_len); test_i++) {
        vector<FLTshare> x_share_vec(vec_len, FLTshare(lf, le)),
            y_share_vec(vec_len, FLTshare(lf, le)), z_share_vec(vec_len, FLTshare(lf, le));
        vector<PI_float_mult_intermediate> intermediate_vec(vec_len,
                                                            PI_float_mult_intermediate(lf, le));

        // preprocess
        for (int i = 0; i < vec_len; i++) {
            FLTshare_preprocess(0, party_id, PRGs, *netio, &x_share_vec[i]);
            FLTshare_preprocess(0, party_id, PRGs, *netio, &y_share_vec[i]);
            PI_float_mult_preprocess(party_id, PRGs, *netio, intermediate_vec[i], &x_share_vec[i],
                                     &y_share_vec[i], &z_share_vec[i]);
        }

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
            x_t &= ShareValue(1) << (lf + 1) - 1;
            x_e &= ShareValue(1) << (le - 1) - 1;
            private_PRG.gen_random_data(&y_b, sizeof(ShareValue));
            private_PRG.gen_random_data(&y_t, sizeof(ShareValue));
            private_PRG.gen_random_data(&y_e, sizeof(ShareValue));
            y_b &= 1;
            y_t &= ShareValue(1) << (lf + 1) - 1;
            y_e &= ShareValue(1) << (le - 1) - 1;

            x_t |= (ShareValue(1) << lf);
            y_t |= (ShareValue(1) << lf);
            x_e += (ShareValue(1) << (le - 2));
            y_e += (ShareValue(1) << (le - 2));
        }
        for (int i = 0; i < vec_len; i++) {
            FLTshare_share_from(0, party_id, *netio, &x_share_vec[i], x_b, x_t, x_e);
            FLTshare_share_from(0, party_id, *netio, &y_share_vec[i], y_b, y_t, y_e);
        }

        // compute
        auto intermediate_ptr = make_ptr_vec(intermediate_vec);
        auto x_share_ptr = make_ptr_vec(x_share_vec);
        auto y_share_ptr = make_ptr_vec(y_share_vec);
        auto z_share_ptr = make_ptr_vec(z_share_vec);
        PI_float_mult_vec(party_id, PRGs, *netio, intermediate_ptr, x_share_ptr, y_share_ptr,
                          z_share_ptr);

        for (int i = 0; i < vec_len; i++) {
            FLTshare &x_share = x_share_vec[i];
            FLTshare &y_share = y_share_vec[i];
            FLTshare &z_share = z_share_vec[i];
            // reveal
            FLTplain x_recon = FLTshare_recon(party_id, *netio, &x_share);
            FLTplain y_recon = FLTshare_recon(party_id, *netio, &y_share);
            FLTplain z_recon = FLTshare_recon(party_id, *netio, &z_share);
            if (party_id == 0) {
                ShareValue z_t_plain = x_t * y_t;
                bool z_wrap = (z_t_plain & (ShareValue(1) << (lf - 1))) ? 1 : 0;
                z_t_plain >>= lf;
                z_t_plain += z_wrap;
                z_t_plain &= ShareValue((ShareValue(1) << (lf + 2)) - 1);

                ShareValue z_e_plain = x_e + y_e - (ShareValue(1) << (le - 1));
                if (z_t_plain & (ShareValue(1) << (lf + 1))) {
                    z_wrap = (z_t_plain & 1) ? 1 : 0;
                    z_t_plain >>= 1;
                    z_e_plain += 1;
                    z_t_plain += z_wrap;
                }
                z_e_plain &= ShareValue((ShareValue(1) << le) - 1);

                ShareValue z_b_plain = (x_b + y_b) % 2;

                bool flag[5] = {0};

                if (z_recon.b != z_b_plain) {
                    flag[0] = 1;
                }

                ShareValue dif =
                    z_t_plain > z_recon.t ? z_t_plain - z_recon.t : z_recon.t - z_t_plain;
                if (dif > 2) {
                    flag[1] = 1;
                }

                if (z_recon.e != z_e_plain) {
                    flag[2] = 1;
                }

                // 检测明文最高位1的位置是否正常
                if ((z_t_plain & (ShareValue(1) << (lf + 1))) ||
                    !(z_t_plain & (ShareValue(1) << lf))) {
                    flag[3] = 1;
                }

                // 检测重构结果最高位1的位置是否正常
                if ((z_recon.t & (ShareValue(1) << (lf + 1))) ||
                    !(z_recon.t & (ShareValue(1) << lf))) {
                    flag[4] = 1;
                }

                for (int i = 0; i < 5; i++) {
                    if (flag[i]) {
                        cout << "Test " << test_i << " failed!" << endl;
                        cout << "Flag: ";
                        for (int j = 0; j < 5; j++) {
                            cout << flag[j] << " ";
                        }
                        cout << endl;
                        cout << "dif: " << dif << endl;

                        cout << endl << "Info of x:" << endl;
                        cout << "b        :" << x_b << endl;
                        cout << "t        :" << bitset<lf + 2>(x_t) << endl;
                        cout << "e        :" << bitset<le>(x_e) << endl;

                        cout << endl << "Info of y:" << endl;
                        cout << "b        :" << y_b << endl;
                        cout << "t        :" << bitset<lf + 2>(y_t) << endl;
                        cout << "e        :" << bitset<le>(y_e) << endl;

                        cout << endl << "Info of z:" << endl;
                        cout << "b        :" << z_b_plain << endl;
                        cout << "t        :" << bitset<lf + 2>(z_t_plain) << endl;
                        cout << "e        :" << bitset<le>(z_e_plain) << endl;

                        cout << endl << "Reconstructed x:" << endl;
                        cout << "b        :" << x_recon.b << endl;
                        cout << "t        :" << bitset<lf + 2>(x_recon.t) << endl;
                        cout << "e        :" << bitset<le>(x_recon.e) << endl;

                        cout << endl << "Reconstructed y:" << endl;
                        cout << "b        :" << y_recon.b << endl;
                        cout << "t        :" << bitset<lf + 2>(y_recon.t) << endl;
                        cout << "e        :" << bitset<le>(y_recon.e) << endl;

                        cout << endl << "Reconstructed z:" << endl;
                        cout << "b        :" << z_recon.b << endl;
                        cout << "t        :" << bitset<lf + 2>(z_recon.t) << endl;
                        cout << "e        :" << bitset<le>(z_recon.e) << endl;
                        exit(1);
                    }
                }
            }
        }
    }
    cout << "PI_float_mult_vec test passed!" << endl;
    delete netio;
}