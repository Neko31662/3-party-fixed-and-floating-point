#include "protocol/great.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 64;
const ShareValue k = 67676767;

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

    // 非向量测试
    for (int i = 0; i < test_nums; i++) {
        MSSshare x_share(ell), y_share(ell);
        MSSshare_p b_share{k};
        PI_great_intermediate intermediate(ell, k);

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        PI_great_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &b_share);

        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue plain_x, plain_y;
        if (party_id == 0) {
            private_PRG.gen_random_data(&plain_x, sizeof(ShareValue));
            private_PRG.gen_random_data(&plain_y, sizeof(ShareValue));
            plain_x &= x_share.MASK;
            plain_y &= x_share.MASK;
        }
        MSSshare_share_from(0, party_id, *netio, &x_share, plain_x);
        MSSshare_share_from(0, party_id, *netio, &y_share, plain_y);

        // compute
        PI_great(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &b_share);

        // recon
        ShareValue rec = MSSshare_p_recon(party_id, *netio, &b_share);
        ShareValue expected;
        if ((plain_x & (ShareValue(1) << (ell - 1))) != (plain_y & (ShareValue(1) << (ell - 1)))) {
            expected = (plain_x < plain_y) ? 1 : 0;
        } else {
            ShareValue dif = plain_x - plain_y;
            if (dif & (ShareValue(1) << (ell - 1))) {
                expected = 0;
            } else {
                expected = 1;
            }
        }

        if (rec != expected && party_id == 0) {
            cout << "Test " << i << " failed! Expected: " << expected << ", got: " << rec << endl;
            cout << "plain_x: " << bitset<ell>(plain_x) << endl;
            cout << "plain_y: " << bitset<ell>(plain_y) << endl;
            exit(1);
        }
    }

    // 向量测试
    int vec_size = 10;
    for (int i = 0; i < max(1, test_nums / vec_size); i++) {
        int ell_arr[vec_size];
        ShareValue k_arr[vec_size];
        for (int j = 0; j < vec_size; j++) {
            ell_arr[j] = ell - 10 + j;
            if (ell_arr[j] < 1)
                ell_arr[j] = 1;
            if (ell_arr[j] > ShareValue_BitLength)
                ell_arr[j] = ShareValue_BitLength;
            k_arr[j] = k * j + 10;
        }
        vector<MSSshare> x_share;
        vector<MSSshare> y_share;
        vector<MSSshare_p> b_share;
        vector<PI_great_intermediate> intermediate;
        for (int j = 0; j < vec_size; j++) {
            x_share.emplace_back(ell_arr[j]);
            y_share.emplace_back(ell_arr[j]);
            b_share.emplace_back(k_arr[j]);
            intermediate.emplace_back(ell_arr[j], k_arr[j]);
        }

        // preprocess
        for (int j = 0; j < vec_size; j++) {
            PI_great_intermediate &inter = intermediate[j];
            MSSshare &x_s = x_share[j];
            MSSshare &y_s = y_share[j];
            MSSshare_p &b_s = b_share[j];
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_s);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &y_s);
            PI_great_preprocess(party_id, PRGs, *netio, inter, &x_s, &y_s, &b_s);
        }

        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue plain_x_vec[vec_size], plain_y_vec[vec_size];

        for (int j = 0; j < vec_size; j++) {
            plain_x_vec[j] = 0;
            plain_y_vec[j] = 0;
            auto &plain_x = plain_x_vec[j];
            auto &plain_y = plain_y_vec[j];
            if (party_id == 0) {
                private_PRG.gen_random_data(&plain_x, sizeof(ShareValue));
                private_PRG.gen_random_data(&plain_y, sizeof(ShareValue));
                plain_x &= x_share[j].MASK;
                plain_y &= x_share[j].MASK;
            }
            MSSshare_share_from(0, party_id, *netio, &x_share[j], plain_x);
            MSSshare_share_from(0, party_id, *netio, &y_share[j], plain_y);
        }

        // compute
        auto intermediate_ptrs = make_ptr_vec(intermediate);
        auto x_share_ptrs = make_ptr_vec(x_share);
        auto y_share_ptrs = make_ptr_vec(y_share);
        auto b_share_ptrs = make_ptr_vec(b_share);
        PI_great_vec(party_id, PRGs, *netio, intermediate_ptrs, x_share_ptrs, y_share_ptrs,
                     b_share_ptrs);

        // recon
        ShareValue rec_vec[vec_size];
        for (int j = 0; j < vec_size; j++) {
            auto &plain_x = plain_x_vec[j];
            auto &plain_y = plain_y_vec[j];
            auto &b_s = b_share[j];
            ShareValue &rec = rec_vec[j];
            rec = MSSshare_p_recon(party_id, *netio, &b_s);
            ShareValue expected;
            if ((plain_x & (ShareValue(1) << (ell_arr[j] - 1))) !=
                (plain_y & (ShareValue(1) << (ell_arr[j] - 1)))) {
                expected = (plain_x < plain_y) ? 1 : 0;
            } else {
                ShareValue dif = plain_x - plain_y;
                if (dif & (ShareValue(1) << (ell_arr[j] - 1))) {
                    expected = 0;
                } else {
                    expected = 1;
                }
            }
            if (rec != expected && party_id == 0) {
                cout << "Test " << i << " failed! Expected: " << expected << ", got: " << rec
                     << endl;
                cout << "plain_x: " << bitset<ell>(plain_x) << endl;
                cout << "plain_y: " << bitset<ell>(plain_y) << endl;
                exit(1);
            }
        }
    }
}
