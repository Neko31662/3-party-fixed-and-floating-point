/*
To run this, use:

typedef uint256_t LongShareValue;

in config.h
*/
#include "protocol/floating_point_mult.h"
#include <chrono>
#include <iostream>
#include <omp.h>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
// ------------configs------------
const int test_nums_list[] = {1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15,
                              1 << 16, 1 << 17, 1 << 18};
const pair<int, int> lf_le_list[] = {{23, 8}};
const int global_batch_size = 4096;
const int num_threads = 16;
int test_nums;
int lf;
int le;
// ------------configs------------

void run_one_test(int argc, char **argv) {
#include "test/speed_test_inl/setup.inl"

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
#include "test/speed_test_inl/before_preprocess.inl"

#pragma omp parallel for
    for (int i = 0; i < vec_len / batch_size; i++) {
        int thread_id = omp_get_thread_num();
        NetIOMP *netio = netio_list[thread_id];
        vector<PRGSync> &PRGs = PRGs_list[thread_id];
        for (int vi = i * batch_size; vi < min((i + 1) * batch_size, vec_len); vi++) {
            FLTshare &x_share = x_share_vec[vi];
            FLTshare &y_share = y_share_vec[vi];
            FLTshare &z_share = z_share_vec[vi];
            PI_float_mult_intermediate &intermediate = intermediate_vec[vi];
            FLTshare_preprocess(0, party_id, PRGs, *netio, &x_share);
            FLTshare_preprocess(0, party_id, PRGs, *netio, &y_share);
            PI_float_mult_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share,
                                     &z_share);
        }
        if (party_id == 0) {
            netio->send_stored_data(1);
            netio->send_stored_data(2);
        } else if (party_id == 1) {
            netio->send_stored_data(2);
        }
    }

#include "test/speed_test_inl/after_preprocess.inl"

    // compute
    vector<vector<FLTshare *>> x_share_batch_vec(vec_len / batch_size + 1);
    vector<vector<FLTshare *>> y_share_batch_vec(vec_len / batch_size + 1);
    vector<vector<FLTshare *>> z_share_batch_vec(vec_len / batch_size + 1);
    vector<vector<PI_float_mult_intermediate *>> intermediate_batch_vec(vec_len / batch_size + 1);
    for (int i = 0; i < vec_len / batch_size; i++) {
        int start = i * batch_size;
        int end = std::min((i + 1) * batch_size, vec_len);
        for (int j = start; j < end; j++) {
            x_share_batch_vec[i].push_back(&x_share_vec[j]);
            y_share_batch_vec[i].push_back(&y_share_vec[j]);
            z_share_batch_vec[i].push_back(&z_share_vec[j]);
            intermediate_batch_vec[i].push_back(&intermediate_vec[j]);
        }
    }

#include "test/speed_test_inl/before_compute.inl"

#pragma omp parallel for
    for (int i = 0; i < vec_len / batch_size; i++) {
        int thread_id = omp_get_thread_num();
        NetIOMP *netio = netio_list[thread_id];
        vector<PRGSync> &PRGs = PRGs_list[thread_id];
        PI_float_mult_vec(party_id, PRGs, *netio, intermediate_batch_vec[i], x_share_batch_vec[i],
                          y_share_batch_vec[i], z_share_batch_vec[i]);
    }

#include "test/speed_test_inl/after_compute.inl"

// show results
#include "test/speed_test_inl/show_results.inl"
}

int main(int argc, char **argv) {
    int party_id = atoi(argv[1]);
    cout << "Log of speed_floating_mult_test, party:" << party_id << endl;
    for (int i = 0; i < 9; i++) {
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