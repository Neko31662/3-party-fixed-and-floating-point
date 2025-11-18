/*
To run this, use:

typedef __uint128_t ShareValue;

in config.h
*/

#include "protocol/masked_RSS.h"
#include <chrono>
#include <iostream>
#include <omp.h>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
// ------------configs------------
const int test_nums_list[] = {1 << 12, 1 << 14, 1 << 16, 1 << 18, 1 << 20, 1 << 22, 1 << 24};
const int ell_list[] = {52, 56, 68, 72, 80};
const int global_batch_size = 4096;
const int num_threads = 16;
int test_nums;
int ell;
// ------------configs------------

void run_one_test(int argc, char **argv) {
#include "test/speed_test_inl/setup.inl"

    vector<MSSshare> x_share_vec;
    vector<MSSshare> y_share_vec;
    vector<MSSshare_mul_res> z_share_vec;
    for (int vi = 0; vi < vec_len; vi++) {
        x_share_vec.emplace_back(ell);
        y_share_vec.emplace_back(ell);
        z_share_vec.emplace_back(ell);
    }

    // preprocess

#include "test/speed_test_inl/before_preprocess.inl"

#pragma omp parallel for
    for (int i = 0; i < vec_len / batch_size; i++) {
        int thread_id = omp_get_thread_num();
        NetIOMP *netio = netio_list[thread_id];
        vector<PRGSync> &PRGs = PRGs_list[thread_id];
        for (int vi = i * batch_size; vi < min((i + 1) * batch_size, vec_len); vi++) {
            MSSshare &x_share = x_share_vec[vi];
            MSSshare &y_share = y_share_vec[vi];
            MSSshare_mul_res &z_share = z_share_vec[vi];
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
            MSSshare_mul_res_preprocess(party_id, PRGs, *netio, &z_share, &x_share, &y_share);
        }
        if (party_id == 0) {
            netio->send_stored_data(2);
        }
    }

#include "test/speed_test_inl/after_preprocess.inl"

    // compute
    vector<std::vector<MSSshare *>> x_share_batch_vec(vec_len / batch_size + 1);
    vector<std::vector<MSSshare *>> y_share_batch_vec(vec_len / batch_size + 1);
    vector<std::vector<MSSshare_mul_res *>> z_share_batch_vec(vec_len / batch_size + 1);
    for (int i = 0; i * batch_size < vec_len; i++) {
        int start = i * batch_size;
        int end = std::min((i + 1) * batch_size, vec_len);
        for (int j = start; j < end; j++) {
            x_share_batch_vec[i].push_back(&x_share_vec[j]);
            y_share_batch_vec[i].push_back(&y_share_vec[j]);
            z_share_batch_vec[i].push_back(&z_share_vec[j]);
        }
    }

#include "test/speed_test_inl/before_compute.inl"

#pragma omp parallel for
    for (int i = 0; i < vec_len / batch_size; i++) {
        int thread_id = omp_get_thread_num();
        NetIOMP *netio = netio_list[thread_id];
        vector<PRGSync> &PRGs = PRGs_list[thread_id];
        MSSshare_mul_res_calc_mul_vec(party_id, *netio, z_share_batch_vec[i], x_share_batch_vec[i],
                                      y_share_batch_vec[i]);
    }

#include "test/speed_test_inl/after_compute.inl"

// show results
#include "test/speed_test_inl/show_results.inl"
}

int main(int argc, char **argv) {
    int party_id = atoi(argv[1]);
    cout << "Log of speed_SOTA_test, party:" << party_id << endl;
    for (int i = 6; i < 7; i++) {
        test_nums = test_nums_list[i];
        for (int j = 4; j < 5; j++) {
            ell = ell_list[j];
            cout << "----------------------------------------------------------" << endl;
            cout << "ell = " << ell << ", test_nums = " << test_nums << endl;
            run_one_test(argc, argv);
        }
    }
}