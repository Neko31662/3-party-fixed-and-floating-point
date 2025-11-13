#include "protocol/fixed_point.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
// ------------configs------------
const int test_nums_list[] = {1 << 10, 1 << 12, 1 << 14, 1 << 16, 1 << 18, 1 << 20, 1 << 22};
const pair<int, int> li_lf_list[] = {{4, 8}, {8, 8}, {4, 16}, {8, 16}, {16, 16}};
int test_nums;
int li;
int lf;
int l_res;
// ------------configs------------


void run_one_test(int argc, char **argv) {
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

    int vec_len = test_nums;
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
    auto preprocess_start = std::chrono::high_resolution_clock::now();
    auto preprocess_bytes_start = get_netio_bytes(party_id, *netio);

    for (int vi = 0; vi < vec_len; vi++) {
        MSSshare &x_share = x_share_vec[vi];
        MSSshare &y_share = y_share_vec[vi];
        MSSshare &z_share = z_share_vec[vi];
        PI_fixed_mult_intermediate &intermediate = intermediate_vec[vi];
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        PI_fixed_mult_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share,
                                 &z_share);
        if ((vi & (128 - 1)) == 128 - 1) {
            if (party_id == 0) {
                netio->send_stored_data(2);
            }
        }
    }

    if (party_id == 0) {
        netio->send_stored_data(2);
    }

    auto preprocess_end = std::chrono::high_resolution_clock::now();
    auto preprocess_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(preprocess_end - preprocess_start);
    double preprocess_ms = preprocess_duration.count() / 1000.0;
    auto preprocess_bytes_end = get_netio_bytes(party_id, *netio);
    for (int i = 0; i < NUM_PARTIES; i++) {
        preprocess_bytes_end[i] -= preprocess_bytes_start[i];
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

    auto compute_start = std::chrono::high_resolution_clock::now();
    auto compute_bytes_start = get_netio_bytes(party_id, *netio);
    PI_fixed_mult_vec(party_id, PRGs, *netio, intermediate_ptr, x_share_ptr, y_share_ptr,
                      z_share_ptr);
    auto compute_end = std::chrono::high_resolution_clock::now();
    auto compute_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(compute_end - compute_start);
    double compute_ms = compute_duration.count() / 1000.0;
    auto compute_bytes_end = get_netio_bytes(party_id, *netio);
    for (int i = 0; i < NUM_PARTIES; i++) {
        compute_bytes_end[i] -= compute_bytes_start[i];
    }

    // show results
    cout << "Preprocess : " << std::fixed << std::setprecision(2) << std::setw(10)
         << std::setfill(' ') << preprocess_ms << " ms, " << endl;
    cout << "Communication bytes: " << endl;
    for (int i = 0; i < NUM_PARTIES; i++) {
        if (i != party_id) {
            cout << "P" << party_id << "->P" << i << ": " << std::fixed << std::setw(10)
                 << std::setfill(' ') << preprocess_bytes_end[i] << " bytes; " << endl;
        }
    }
    cout << endl;
    cout << "online     : " << std::fixed << std::setprecision(2) << std::setw(10)
         << std::setfill(' ') << compute_ms << " ms" << endl;
    cout << "Communication bytes: " << endl;
    for (int i = 0; i < NUM_PARTIES; i++) {
        if (i != party_id) {
            cout << "P" << party_id << "->P" << i << ": " << std::fixed << std::setw(10)
                 << std::setfill(' ') << compute_bytes_end[i] << " bytes; " << endl;
        }
    }
}

int main(int argc, char **argv) {
    int party_id = atoi(argv[1]);
    cout << "Log of speed_fixed_mult_test, party:" << party_id << endl;
    for (int i = 0; i < 6; i++) {
        test_nums = test_nums_list[i];
        for (int j = 0; j < 5; j++) {
            li = li_lf_list[j].first;
            lf = li_lf_list[j].second;
            l_res = li + lf;
            cout << "----------------------------------------------------------" << endl;
            cout << "li = " << li << ", lf = " << lf << ", test_nums = " << test_nums << endl;
            run_one_test(argc, argv);
        }
    }
}