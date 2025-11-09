#include "protocol/align.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 32;
const int ell2 = 16;

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

    for (int test_i = 0; test_i < test_nums; test_i++) {
        PI_align_intermediate<ell, ell2> intermediate;
        MSSshare<ell> x_share;
        MSSshare_add_res<ell> z_share;
        MSSshare_mul_res<ell2> zeta_share;
        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        PI_align_preprocess<ell, ell2>(party_id, PRGs, *netio, intermediate, &x_share, &z_share,
                                       &zeta_share);

        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue test_value;
        if (party_id == 0) {
            private_PRG.gen_random_data(&test_value, sizeof(ShareValue));
            test_value &= (ShareValue(1) << ell) - 1;
            ShareValue tmp;
            private_PRG.gen_random_data(&tmp, sizeof(ShareValue));
            tmp %= ell;
            // test_value >>= tmp;
        }
        MSSshare_share_from(0, party_id, *netio, &x_share, test_value);

        // test
        PI_align(party_id, PRGs, *netio, intermediate, &x_share, &z_share, &zeta_share);

        // reconstruct
        ShareValue z_recon = 0;
        z_recon = MSSshare_recon(party_id, *netio, &z_share);
        ShareValue zeta_recon = 0;
        zeta_recon = MSSshare_recon(party_id, *netio, &zeta_share);

        // check
        if (party_id == 0) {
            netio->send_data(1, &test_value, sizeof(ShareValue));
            netio->send_data(2, &test_value, sizeof(ShareValue));
        } else {
            netio->recv_data(0, &test_value, sizeof(ShareValue));
        }
        ShareValue expected_z = test_value;
        int expected_zeta = 0;
        if (test_value != 0) {
            while (expected_z < (ShareValue(1) << ell)) {
                expected_zeta += 1;
                expected_z <<= 1;
            }
            expected_zeta -= 1;
            expected_z >>= 1;
        }

        if (z_recon != expected_z || zeta_recon != expected_zeta) {
            cout << "PI_align test failed at index " << test_i << endl;
            myout.show();
            cout << "x_share.v1: " << bitset<ell>(x_share.v1) << endl;
            cout << "x_share.v2: " << bitset<ell>(x_share.v2) << endl;
            cout << "test_value: " << bitset<ell>(test_value) << endl;
            cout << "z_recon: " << bitset<ell>(z_recon) << endl;
            cout << "zeta_recon: " << zeta_recon << endl;
            cout << "expected_z: " << bitset<ell>(expected_z) << endl;
            cout << "expected_zeta: " << expected_zeta << endl;
            exit(1);
        } else {
            myout.clear_buffer();
        }
    }
    cout << "PI_align test passed!" << endl;
    delete netio;
    return 0;
}