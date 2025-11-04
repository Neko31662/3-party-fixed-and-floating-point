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

    for (int i = 0; i < test_nums; i++) {
        // generate bit shares
        std::vector<ShareValue> bit_shares(l);
        std::vector<bool> plain_bits(l);
        if (party_id != 0) {
            for (int i = 0; i < l; i++) {
                ShareValue tmp;
                PRGs[1].gen_random_data(&tmp, sizeof(ShareValue));
                bit_shares[i] = party_id == 1 ? tmp % 2 : 0;
            }
            if (party_id == 1) {
                for (int i = 0; i < l; i++) {
                    plain_bits[i] = bit_shares[i];
                }
            }
            for (int i = 0; i < l; i++) {
                ShareValue tmp;
                PRGs[1].gen_random_data(&tmp, sizeof(ShareValue));
                if (party_id == 1) {
                    bit_shares[i] = (bit_shares[i] + tmp) % p1;
                } else {
                    bit_shares[i] = (bit_shares[i] + p1 - (tmp % p1)) % p1;
                }
            }
            // log("Bit shares:");
            // for (int i = 0; i < l; i++) {
            //     std::cout << std::setw(3) << bit_shares[i] << ' ';
            // }
            // log("");
        }

        ShareValue output_result = 0;
        PI_detect(party_id, PRGs, *netio, bit_shares, p1, p2, output_result);
        if (party_id == 2) {
            netio->send_data(1, &output_result, sizeof(output_result));
        } else if (party_id == 1) {
            ShareValue tmp;
            netio->recv_data(2, &tmp, sizeof(output_result));
            output_result = (output_result + tmp) % p2;
        }

        // check result
        if (party_id == 1) {
            for (int i = 0; i < output_result; i++) {
                if (plain_bits[i] == 1) {
                    std::cout << "Error: the first not zero index is " << i << ", but got "
                              << (uint64_t)output_result << std::endl;
                    return -1;
                }
            }
            if (plain_bits[output_result] == 0) {
                std::cout << "Error: the first not zero index is not " << (uint64_t)output_result
                          << std::endl;
                return -1;
            }
        }
    }
    std::cout << "PI_detect test passed!" << std::endl;
    delete netio;
}
