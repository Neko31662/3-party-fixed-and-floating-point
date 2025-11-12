#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int vec_len = 20;

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

    // mod 2^l，非向量
    for (int test_i = 0; test_i < test_nums; test_i++) {
        const int ell = 60;
        const int len = 10;
        // circuit: (a+b) * (c*d) + (e*f) + g - 10
        ShareValue secret[len];
        std::vector<MSSshare> s(len, MSSshare(ell));
        std::vector<MSSshare> s_add(len, MSSshare(ell));
        std::vector<MSSshare_mul_res> s_mul(len, MSSshare_mul_res(ell));

        // preprocess
        //    i: 0, 1, 2, 3, 4, 5, 6
        // s[i]: a, b, c, d, e, f, g
        for (int i = 0; i < (int)s.size(); i++) {
            MSSshare_preprocess(i % 3, party_id, PRGs, *netio, &s[i]);
        }
        //        i: 0,   1,                     2
        // s_add[i]: a+b, (a+b) * (c*d) + (e*f), (a+b) * (c*d) + (e*f) + g
        // ======================================================================
        //        i: 0,   1,   2
        // s_mul[i]: c*d, e*f, (a+b) * (c*d)
        MSSshare_add_res_preprocess(party_id, &s_add[0], &s[0], &s[1]);
        MSSshare_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[0], &s[2], &s[3]);
        MSSshare_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[1], &s[4], &s[5]);
        MSSshare_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[2], &s_mul[0], &s_add[0]);
        MSSshare_add_res_preprocess(party_id, &s_add[1], &s_mul[2], &s_mul[1]);
        MSSshare_add_res_preprocess(party_id, &s_add[2], &s_add[1], &s[6]);

        // preprocess后需要调用这个
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        for (int i = 0; i < len; i++) {
            public_PRG.gen_random_data(&secret[i], sizeof(ShareValue));
            secret[i] &= s[0].MASK;
        }
        for (int holder = 0; holder < 3; holder++) {
            for (int i = 0; i < 7; i++) {
                if (i % 3 == holder)
                    MSSshare_share_from(i % 3, party_id, *netio, &s[i], secret[i]);
            }
        }

        // calculate
        MSSshare_add_res_calc_add(party_id, &s_add[0], &s[0], &s[1]);
        MSSshare_mul_res_calc_mul(party_id, *netio, &s_mul[0], &s[2], &s[3]);
        MSSshare_mul_res_calc_mul(party_id, *netio, &s_mul[1], &s[4], &s[5]);
        MSSshare_mul_res_calc_mul(party_id, *netio, &s_mul[2], &s_mul[0], &s_add[0]);
        MSSshare_add_res_calc_add(party_id, &s_add[1], &s_mul[2], &s_mul[1]);
        MSSshare_add_res_calc_add(party_id, &s_add[2], &s_add[1], &s[6]);
        MSSshare_add_plain(party_id, &s_add[2], ShareValue(-10));

        // reconstruct
        ShareValue rec[len];
        ShareValue rec_add[len];
        ShareValue rec_mul[len];
        for (int i = 0; i <= 6; i++) {
            rec[i] = MSSshare_recon(party_id, *netio, &s[i]);
        }
        for (int i = 0; i <= 2; i++) {
            rec_add[i] = MSSshare_recon(party_id, *netio, &s_add[i]);
        }
        for (int i = 0; i <= 2; i++) {
            rec_mul[i] = MSSshare_recon(party_id, *netio, &s_mul[i]);
        }

        ShareValue res = ((secret[0] + secret[1]) * (secret[2] * secret[3]) +
                          (secret[4] * secret[5]) + secret[6] - 10);
        if (ell < ShareValue_BitLength)
            res %= (1ULL << ell);
        if (rec_add[2] != res) {
            cout << "Reconstructed result: " << (uint64_t)rec_add[2] << endl;
            cout << "Expected result: " << (uint64_t)res << endl;
            cout << "MSS basic test failed!" << endl;
            exit(1);
        }
    }

    // mod 2^l，向量
    for (int test_i = 0; test_i < max(1, test_nums / vec_len); test_i++) {
        const int ell = 40;
        ShareValue secret_s1[vec_len], secret_s2[vec_len];
        vector<int> ell_list(vec_len);
        for (int i = 0; i < vec_len; i++) {
            ell_list[i] = min(64, ell + i);
        }
        std::vector<MSSshare> s1, s2;
        std::vector<MSSshare_mul_res> s_mul;
        for (int i = 0; i < vec_len; i++) {
            s1.push_back(MSSshare(ell_list[i]));
            s2.push_back(MSSshare(ell_list[i]));
            s_mul.push_back(MSSshare_mul_res(ell_list[i]));
        }

        // preprocess
        for (int i = 0; i < vec_len; i++) {
            MSSshare_preprocess(i % 3, party_id, PRGs, *netio, &s1[i]);
            MSSshare_preprocess(i % 3, party_id, PRGs, *netio, &s2[i]);
            MSSshare_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[i], &s1[i], &s2[i]);
        }

        // preprocess后需要调用这个
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        for (int i = 0; i < vec_len; i++) {
            public_PRG.gen_random_data(&secret_s1[i], sizeof(ShareValue));
            secret_s1[i] &= s1[i].MASK;
            public_PRG.gen_random_data(&secret_s2[i], sizeof(ShareValue));
            secret_s2[i] &= s2[i].MASK;
        }
        for (int holder = 0; holder < 3; holder++) {
            for (int i = 0; i < vec_len; i++) {
                if (i % 3 == holder) {
                    MSSshare_share_from(i % 3, party_id, *netio, &s1[i], secret_s1[i]);
                    MSSshare_share_from(i % 3, party_id, *netio, &s2[i], secret_s2[i]);
                }
            }
        }

        // calculate
        auto s_mul_vec = make_ptr_vec(s_mul);
        auto s1_vec = make_ptr_vec(s1);
        auto s2_vec = make_ptr_vec(s2);
        MSSshare_mul_res_calc_mul_vec(party_id, *netio, s_mul_vec, s1_vec, s2_vec);

        // reconstruct
        ShareValue rec_s1[vec_len];
        ShareValue rec_s2[vec_len];
        ShareValue rec_mul[vec_len];
        for (int i = 0; i < vec_len; i++) {
            rec_s1[i] = MSSshare_recon(party_id, *netio, &s1[i]);
            rec_s2[i] = MSSshare_recon(party_id, *netio, &s2[i]);
            rec_mul[i] = MSSshare_recon(party_id, *netio, &s_mul[i]);
        }
        ShareValue plain_mul[vec_len];
        for (int i = 0; i < vec_len; i++) {
            plain_mul[i] = (secret_s1[i] * secret_s2[i]) & s1[i].MASK;
        }
        for (int i = 0; i < vec_len; i++) {
            if (rec_mul[i] != plain_mul[i]) {
                cout << "Vectorized test failed at test_i = " << test_i << endl;
                cout << "i = " << i << " failed!" << endl;
                cout << "Reconstructed mul: " << (uint64_t)rec_mul[i] << endl;
                cout << "Expected mul: " << (uint64_t)plain_mul[i] << endl;
                cout << "MSS vectorized mul test failed!" << endl;
                exit(1);
            }
        }
    }

    // mod p，非向量
    for (int test_i = 0; test_i < test_nums; test_i++) {
        const int p = 169316931;
        const int len = 10;
        // circuit: (a+b) * (c*d) + (e*f) + g - 10
        ShareValue secret[len];
        std::vector<MSSshare_p> s(len, MSSshare_p(p));
        std::vector<MSSshare_p> s_add(len, MSSshare_p(p));
        std::vector<MSSshare_p_mul_res> s_mul(len, MSSshare_p_mul_res(p));

        // preprocess
        //    i: 0, 1, 2, 3, 4, 5, 6
        // s[i]: a, b, c, d, e, f, g
        for (int i = 0; i < (int)s.size(); i++) {
            MSSshare_p_preprocess(i % 3, party_id, PRGs, *netio, &s[i]);
        }
        //        i: 0,   1,                     2
        // s_add[i]: a+b, (a+b) * (c*d) + (e*f), (a+b) * (c*d) + (e*f) + g
        // ======================================================================
        //        i: 0,   1,   2
        // s_mul[i]: c*d, e*f, (a+b) * (c*d)
        MSSshare_p_add_res_preprocess(party_id, &s_add[0], &s[0], &s[1]);
        MSSshare_p_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[0], &s[2], &s[3]);
        MSSshare_p_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[1], &s[4], &s[5]);
        MSSshare_p_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[2], &s_mul[0], &s_add[0]);
        MSSshare_p_add_res_preprocess(party_id, &s_add[1], &s_mul[2], &s_mul[1]);
        MSSshare_p_add_res_preprocess(party_id, &s_add[2], &s_add[1], &s[6]);

        // preprocess后需要调用这个
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        for (int i = 0; i < len; i++) {
            public_PRG.gen_random_data(&secret[i], sizeof(ShareValue));
            secret[i] %= s[0].p;
        }
        for (int holder = 0; holder < 3; holder++) {
            for (int i = 0; i < 7; i++) {
                if (i % 3 == holder)
                    MSSshare_p_share_from(i % 3, party_id, *netio, &s[i], secret[i]);
            }
        }

        // calculate
        MSSshare_p_add_res_calc_add(party_id, &s_add[0], &s[0], &s[1]);
        MSSshare_p_mul_res_calc_mul(party_id, *netio, &s_mul[0], &s[2], &s[3]);
        MSSshare_p_mul_res_calc_mul(party_id, *netio, &s_mul[1], &s[4], &s[5]);
        MSSshare_p_mul_res_calc_mul(party_id, *netio, &s_mul[2], &s_mul[0], &s_add[0]);
        MSSshare_p_add_res_calc_add(party_id, &s_add[1], &s_mul[2], &s_mul[1]);
        MSSshare_p_add_res_calc_add(party_id, &s_add[2], &s_add[1], &s[6]);
        MSSshare_p_add_plain(party_id, &s_add[2], ShareValue(-10));

        // reconstruct
        ShareValue rec[len];
        ShareValue rec_add[len];
        ShareValue rec_mul[len];
        for (int i = 0; i <= 6; i++) {
            rec[i] = MSSshare_p_recon(party_id, *netio, &s[i]);
        }
        for (int i = 0; i <= 2; i++) {
            rec_add[i] = MSSshare_p_recon(party_id, *netio, &s_add[i]);
        }
        for (int i = 0; i <= 2; i++) {
            rec_mul[i] = MSSshare_p_recon(party_id, *netio, &s_mul[i]);
        }

        ShareValue res = ((secret[0] + secret[1]) * ((secret[2] * secret[3]) % p) +
                          (secret[4] * secret[5]) + secret[6] - 10 + 10 * p) %
                         p;
        if (rec_add[2] != res) {
            cout << "Reconstructed result: " << (uint64_t)rec_add[2] << endl;
            cout << "Expected result: " << (uint64_t)res << endl;
            cout << "MSS_p basic test failed!" << endl;
            exit(1);
        }
    }

    // mod p，向量
    for (int test_i = 0; test_i < max(1, test_nums / vec_len); test_i++) {
        const ShareValue p = 6;
        ShareValue secret_s1[vec_len], secret_s2[vec_len];
        vector<int> p_list(vec_len);
        for (int i = 0; i < vec_len; i++) {
            p_list[i] = p + i * 100;
        }
        std::vector<MSSshare_p> s1, s2;
        std::vector<MSSshare_p_mul_res> s_mul;
        for (int i = 0; i < vec_len; i++) {
            s1.push_back(MSSshare_p(p_list[i]));
            s2.push_back(MSSshare_p(p_list[i]));
            s_mul.push_back(MSSshare_p_mul_res(p_list[i]));
        }

        // preprocess
        for (int i = 0; i < vec_len; i++) {
            MSSshare_p_preprocess(i % 3, party_id, PRGs, *netio, &s1[i]);
            MSSshare_p_preprocess(i % 3, party_id, PRGs, *netio, &s2[i]);
            MSSshare_p_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[i], &s1[i], &s2[i]);
        }

        // preprocess后需要调用这个
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        for (int i = 0; i < vec_len; i++) {
            public_PRG.gen_random_data(&secret_s1[i], sizeof(ShareValue));
            secret_s1[i] %= s1[i].p;
            public_PRG.gen_random_data(&secret_s2[i], sizeof(ShareValue));
            secret_s2[i] %= s2[i].p;
        }
        for (int holder = 0; holder < 3; holder++) {
            for (int i = 0; i < vec_len; i++) {
                if (i % 3 == holder) {
                    MSSshare_p_share_from(i % 3, party_id, *netio, &s1[i], secret_s1[i]);
                    MSSshare_p_share_from(i % 3, party_id, *netio, &s2[i], secret_s2[i]);
                }
            }
        }

        // calculate
        auto s_mul_vec = make_ptr_vec(s_mul);
        auto s1_vec = make_ptr_vec(s1);
        auto s2_vec = make_ptr_vec(s2);
        MSSshare_p_mul_res_calc_mul_vec(party_id, *netio, s_mul_vec, s1_vec, s2_vec);

        // reconstruct
        ShareValue rec_s1[vec_len];
        ShareValue rec_s2[vec_len];
        ShareValue rec_mul[vec_len];
        for (int i = 0; i < vec_len; i++) {
            rec_s1[i] = MSSshare_p_recon(party_id, *netio, &s1[i]);
            rec_s2[i] = MSSshare_p_recon(party_id, *netio, &s2[i]);
            rec_mul[i] = MSSshare_p_recon(party_id, *netio, &s_mul[i]);
        }
        ShareValue plain_mul[vec_len];
        for (int i = 0; i < vec_len; i++) {
            plain_mul[i] = (secret_s1[i] * secret_s2[i]) % s1[i].p;
        }
        for (int i = 0; i < vec_len; i++) {
            if (rec_mul[i] != plain_mul[i]) {
                cout << "Vectorized test failed at test_i = " << test_i << endl;
                cout << "i = " << i << " failed!" << endl;
                cout << "Reconstructed mul: " << (uint64_t)rec_mul[i] << endl;
                cout << "Expected mul: " << (uint64_t)plain_mul[i] << endl;
                cout << "MSS vectorized mul test failed!" << endl;
                exit(1);
            }
        }
    }

    cout << "All test passed!" << endl;
    delete netio;
    return 0;
}