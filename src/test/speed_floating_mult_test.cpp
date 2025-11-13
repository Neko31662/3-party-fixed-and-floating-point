#include "protocol/floating_point_mult.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
// ------------configs------------
const int test_nums_list[] = {1 << 7, 1 << 8, 1 << 9, 1 << 10, 1 << 11, 1 << 12, 1 << 13};
const pair<int, int> lf_le_list[] = {{23, 8}};
int test_nums;
int lf;
int le;
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
    vector<FLTshare> x_share_vec;
    vector<FLTshare> y_share_vec;
    vector<FLTshare> z_share_vec;
    vector<PI_float_mult_intermediate> intermediate_vec;
    for (int vi = 0; vi < vec_len; vi++) {
        x_share_vec.emplace_back(lf, le);
        y_share_vec.emplace_back(lf, le);
        z_share_vec.emplace_back(lf, le);
        intermediate_vec.emplace_back(lf, le);
    }

    // preprocess
    auto preprocess_start = std::chrono::high_resolution_clock::now();
    auto preprocess_bytes_start = get_netio_bytes(party_id, *netio);

    for (int vi = 0; vi < vec_len; vi++) {
        FLTshare &x_share = x_share_vec[vi];
        FLTshare &y_share = y_share_vec[vi];
        FLTshare &z_share = z_share_vec[vi];
        PI_float_mult_intermediate &intermediate = intermediate_vec[vi];
        FLTshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        FLTshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        PI_float_mult_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share,
                                 &z_share);
        if ((vi & (128 - 1)) == 128 - 1) {
            if (party_id == 0 || party_id == 1) {
                netio->send_stored_data(2);
            }
        }
    }

    if (party_id == 0 || party_id == 1) {
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
    for (int vi = 0; vi < vec_len; vi++) {
        FLTshare &x_share = x_share_vec[vi];
        FLTshare &y_share = y_share_vec[vi];
        FLTshare &z_share = z_share_vec[vi];
        PI_float_mult_intermediate &intermediate = intermediate_vec[vi];
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
    }

    // compute
    auto compute_start = std::chrono::high_resolution_clock::now();
    auto compute_bytes_start = get_netio_bytes(party_id, *netio);

    for (int i = 0; i < vec_len; i++) {
        PI_float_mult(party_id, PRGs, *netio, intermediate_vec[i], &x_share_vec[i], &y_share_vec[i],
                      &z_share_vec[i]);
    }

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
    cout << "Log of speed_floating_mult_test, party:" << party_id << endl;
    for (int i = 0; i < 5; i++) {
        test_nums = test_nums_list[i];
        for (int j = 0; j < 1; j++) {
            lf = lf_le_list[j].first;
            le = lf_le_list[j].second;
            cout << "----------------------------------------------------------" << endl;
            cout << "lf = " << lf << ", le = " << le << ", test_nums = " << test_nums << endl;
            run_one_test(argc, argv);
        }
    }
}