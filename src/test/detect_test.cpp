#include "protocol/detect.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const ShareValue p1 = 67;
const ShareValue p2 = 67;
const int l = 20;
const int test_nums = 1000;
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

    for (int test_i = 0; test_i < test_nums; test_i++) {
        // generate bit shares
        std::vector<ADDshare_p> bit_shares(l, p1);
        std::vector<bool> plain_bits(l);
        if (party_id != 0) {
            for (int i = 0; i < l; i++) {
                ShareValue tmp;
                PRGs[1].gen_random_data(&tmp, sizeof(ShareValue));
                plain_bits[i] = tmp % 2;
            }
        }
        for (int i = 0; i < l; i++) {
            ADDshare_p_share_from(2, party_id, PRGs, *netio, &bit_shares[i], plain_bits[i]);
        }
        ADDshare_p output_result(p2);

        // compute
        PI_detect(party_id, PRGs, *netio, bit_shares, &output_result);

        // reconstruct result
        ShareValue output_result_val = ADDshare_p_recon(party_id, *netio, &output_result);

        // check result
        if (party_id == 1) {
            for (int i = 0; i < output_result_val; i++) {
                if (plain_bits[i] == 1) {
                    std::cout << "Error: the first not zero index is " << i << ", but got "
                              << output_result_val << std::endl;
                    return -1;
                }
            }
            if (plain_bits[output_result_val] == 0) {
                std::cout << "Error: the first not zero index is not " << output_result_val
                          << std::endl;
                return -1;
            }
        }
    }
    std::cout << "PI_detect test passed!" << std::endl;

    for (int test_i = 0; test_i < max(1, test_nums / vec_len); test_i++) {
        // generate bit shares
        vector<vector<ADDshare_p>> bit_shares_vec(vec_len, vector<ADDshare_p>(l, p1));
        vector<vector<bool>> plain_bits_vec(vec_len, vector<bool>(l));
        vector<ADDshare_p> output_result_vec(vec_len, p2);

        // share
        for (int idx = 0; idx < vec_len; idx++) {
            if (party_id != 0) {
                for (int i = 0; i < l; i++) {
                    ShareValue tmp;
                    PRGs[1].gen_random_data(&tmp, sizeof(ShareValue));
                    plain_bits_vec[idx][i] = tmp % 2;
                }
            }
            for (int i = 0; i < l; i++) {
                ADDshare_p_share_from(2, party_id, PRGs, *netio, &bit_shares_vec[idx][i],
                                      plain_bits_vec[idx][i]);
            }
        }

        // compute
        auto bit_shares_ptr = make_ptr_vec(bit_shares_vec);
        auto output_result_ptr = make_ptr_vec(output_result_vec);
        PI_detect_vec(party_id, PRGs, *netio, bit_shares_ptr, output_result_ptr);

        // reconstruct result
        for (int idx = 0; idx < vec_len; idx++) {
            ShareValue output_result_val = ADDshare_p_recon(party_id, *netio, &output_result_vec[idx]);
            if (party_id == 1) {
                for (int i = 0; i < output_result_val; i++) {
                    if (plain_bits_vec[idx][i] == 1) {
                        std::cout << "Error: the first not zero index is " << i << ", but got "
                                  << output_result_val << std::endl;
                        return -1;
                    }
                }
                if (plain_bits_vec[idx][output_result_val] == 0) {
                    std::cout << "Error: the first not zero index is not " << output_result_val
                              << std::endl;
                    return -1;
                }
            }
        }
    }
    std::cout << "PI_detect_vec test passed!" << std::endl;
    delete netio;
}
