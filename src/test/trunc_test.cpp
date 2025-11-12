#include "protocol/trunc.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 63;

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
        MSSshare x_share(ell);
        MSSshare z_share(ell);
        PI_trunc_intermediate intermediate(ell);
        int bits;
        bits = test_i % (ell + 5) + 1;

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        PI_trunc_preprocess(party_id, PRGs, *netio, intermediate, &x_share, bits, &z_share);

        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue test_value;
        if (party_id == 0) {
            private_PRG.gen_random_data(&test_value, sizeof(ShareValue));
            test_value &= x_share.MASK;
        }
        MSSshare_share_from(0, party_id, *netio, &x_share, test_value);

        // compute
        PI_trunc(party_id, PRGs, *netio, intermediate, &x_share, bits, &z_share);

        // reveal
        ShareValue x_recon = MSSshare_recon(party_id, *netio, &x_share);
        ShareValue z_recon = MSSshare_recon(party_id, *netio, &z_share);
        ShareValue z_plain = ((bits < ell) ? (test_value >> bits) : 0) & x_share.MASK;
        ShareValue dif = z_plain > z_recon ? z_plain - z_recon : z_recon - z_plain;
        if (dif > 1 && party_id == 0) {
            // 排除特殊情况：00000000 和 11111111
            ShareValue mx = max(z_plain, z_recon), mn = min(z_plain, z_recon);
            if (!(mn == 0 && mx == z_share.MASK)) {
                cout << "Test " << test_i << " failed!" << endl;
                cout << "bits: " << bits << endl;
                cout << "x_recon: " << bitset<ell>(x_recon) << endl;
                cout << "z_recon: " << bitset<ell>(z_recon) << endl;
                cout << "z_plain: " << bitset<ell>(z_plain) << endl;
                exit(1);
            }
        }
    }
    cout << "PI_trunc test passed!" << endl;
    delete netio;
    return 0;
}