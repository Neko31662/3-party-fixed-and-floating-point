#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;

const int len = 20;
// circuit: (a+b) * (c*d) + (e*f) + g - 10
uint64_t secret[10] = {1111, 2222, 3333, 4444, 5555, 6666, 7777};

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

    for (int test_round = 0; test_round < 2; test_round++) {
        // mod 2^l
        {
            const int ell = 60;
            // shares allocation
            std::vector<MSSshare<ell>> s(len);
            std::vector<MSSshare_add_res<ell>> s_add(len);
            std::vector<MSSshare_mul_res<ell>> s_mul(len);

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

            // show result
            // for (int i = 0; i <= 6; i++) {
            //     cout << "Reconstructed s[" << i << "]: " << (uint64_t)rec[i] << endl;
            // }
            // for (int i = 0; i <= 2; i++) {
            //     cout << "Reconstructed s_add[" << i << "]: " << (uint64_t)rec_add[i] << endl;
            // }
            // for (int i = 0; i <= 2; i++) {
            //     cout << "Reconstructed s_mul[" << i << "]: " << (uint64_t)rec_mul[i] << endl;
            // }
            ShareValue res = ((secret[0] + secret[1]) * (secret[2] * secret[3]) +
                              (secret[4] * secret[5]) + secret[6] - 10);
            if (ell < ShareValue_BitLength)
                res %= (1ULL << ell);
            cout << "Reconstructed result: " << (uint64_t)rec_add[2] << endl;
            cout << "Expected result: " << (uint64_t)res << endl;
            if (rec_add[2] == res) {
                cout << "MSS basic test passed!" << endl;
            } else {
                cout << "MSS basic test failed!" << endl;
                return 1;
            }
        }

        // mod p
        {
            const int p = 169316931;
            // shares allocation
            std::vector<MSSshare_p> s(len, p);
            std::vector<MSSshare_p_add_res> s_add(len, p);
            std::vector<MSSshare_p_mul_res> s_mul(len, p);

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

            // show result
            // for (int i = 0; i <= 6; i++) {
            //     cout << "Reconstructed s[" << i << "]: " << (uint64_t)rec[i] << endl;
            // }
            // for (int i = 0; i <= 2; i++) {
            //     cout << "Reconstructed s_add[" << i << "]: " << (uint64_t)rec_add[i] << endl;
            // }
            // for (int i = 0; i <= 2; i++) {
            //     cout << "Reconstructed s_mul[" << i << "]: " << (uint64_t)rec_mul[i] << endl;
            // }
            ShareValue res = ((secret[0] + secret[1]) * (secret[2] * secret[3]) +
                              (secret[4] * secret[5]) + secret[6] - 10) %
                             p;
            cout << "Reconstructed result: " << (uint64_t)rec_add[2] << endl;
            cout << "Expected result: " << (uint64_t)res << endl;
            if (rec_add[2] == res) {
                cout << "MSS_p basic test passed!" << endl;
            } else {
                cout << "MSS_p basic test failed!" << endl;
                return 1;
            }
        }
    }
    delete netio;
    return 0;
}