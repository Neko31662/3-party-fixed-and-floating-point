#include "protocol/detect.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const ShareValue p1 = 67;
const ShareValue p2 = (1LL << 32);
const int l = 20;
const int test_nums = 1000;

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

    // b缺省
    {
        for (int i = 0; i < test_nums; i++) {
            // generate bit shares
            std::vector<ADDshare_p> bit_shares(l, p1);
            std::vector<ADDshare_p> output_result(l, p2);
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
            vector<ADDshare_p *> bit_share_ptrs = make_ptr_vec(bit_shares);
            vector<ADDshare_p *> output_result_ptrs = make_ptr_vec(output_result);

            // compute
            PI_prefix(party_id, PRGs, *netio, bit_share_ptrs, output_result_ptrs);

            // reconstruct result
            vector<ShareValue> plain_output_result(l);
            for (int i = 0; i < l; i++) {
                plain_output_result[i] = ADDshare_p_recon(party_id, *netio, &output_result[i]);
            }

            // check result
            if (party_id == 1) {
                int first_one_index = l;
                for (int i = 0; i < l; i++) {
                    if (plain_bits[i]) {
                        first_one_index = i;
                        break;
                    }
                }
                for (int i = 0; i < l; i++) {
                    if (i <= first_one_index) {
                        if (plain_output_result[i] != 1) {
                            std::cout << "Error at test " << i
                                      << ": prefix result incorrect at index " << i << std::endl;
                            return -1;
                        }
                    } else {
                        if (plain_output_result[i] != 0) {
                            std::cout << "Error at test " << i
                                      << ": prefix result incorrect at index " << i << std::endl;
                            return -1;
                        }
                    }
                }
            }
        }
    }
    std::cout << "PI_prefix test passed!" << std::endl;

    // b提供
    {
        for (int i = 0; i < test_nums; i++) {
            // generate bit shares
            std::vector<ADDshare_p> bit_shares(l, p1);
            std::vector<ADDshare_p> output_result(l, p2);
            std::vector<bool> plain_bits(l);
            MSSshare_p b(p2);

            // preprocess
            MSSshare_p_preprocess(0, party_id, PRGs, *netio, &b);

            // share
            ShareValue plain_b = i;
            MSSshare_p_share_from(0, party_id, *netio, &b, plain_b);
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
            vector<ADDshare_p *> bit_share_ptrs = make_ptr_vec(bit_shares);
            vector<ADDshare_p *> output_result_ptrs = make_ptr_vec(output_result);

            // compute
            PI_prefix_b(party_id, PRGs, *netio, bit_share_ptrs, &b, output_result_ptrs);

            // reconstruct result
            vector<ShareValue> plain_output_result(l);
            for (int i = 0; i < l; i++) {
                plain_output_result[i] = ADDshare_p_recon(party_id, *netio, &output_result[i]);
            }

            // check result
            if (party_id == 1) {
                int first_one_index = l;
                for (int i = 0; i < l; i++) {
                    if (plain_bits[i]) {
                        first_one_index = i;
                        break;
                    }
                }
                for (int i = 0; i < l; i++) {
                    if (i <= first_one_index) {
                        if (plain_output_result[i] != (plain_b % p2)) {
                            std::cout << "Error at test " << i
                                      << ": prefix_b result incorrect at index " << i << std::endl;
                            return -1;
                        }
                    } else {
                        if (plain_output_result[i] != 0) {
                            std::cout << "Error at test " << i
                                      << ": prefix_b result incorrect at index " << i << std::endl;
                            return -1;
                        }
                    }
                }
            }
        }
    }

    std::cout << "PI_prefix_b test passed!" << std::endl;
    delete netio;
}
