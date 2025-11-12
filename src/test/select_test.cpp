#include "protocol/select.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 63;
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

    for (int test_i = 0; test_i < test_nums; test_i++) {
        PI_select_intermediate intermediate(ell);
        MSSshare x_share(ell), y_share(ell);
        MSSshare z_share(ell);
        MSSshare b_share(1);

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &b_share);
        PI_select_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &b_share,
                             &z_share);

        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue x_plain, y_plain, b_plain;
        if (party_id == 0) {
            private_PRG.gen_random_data(&x_plain, sizeof(ShareValue));
            x_plain &= x_share.MASK;
            private_PRG.gen_random_data(&y_plain, sizeof(ShareValue));
            y_plain &= x_share.MASK;
            private_PRG.gen_random_data(&b_plain, sizeof(ShareValue));
            b_plain &= 1;
        }
        MSSshare_share_from(0, party_id, *netio, &x_share, x_plain);
        MSSshare_share_from(0, party_id, *netio, &y_share, y_plain);
        MSSshare_share_from(0, party_id, *netio, &b_share, b_plain);

        // compute
        PI_select(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &b_share, &z_share);

        // reveal
        ShareValue z_recon = MSSshare_recon(party_id, *netio, &z_share);

        // check
        if (party_id == 0) {
            ShareValue expected = b_plain ? x_plain : y_plain;
            if (z_recon != expected) {
                cout << "Test " << test_i << " failed at index " << test_i << endl;
                cout << "Expected: " << bitset<ell>(expected) << endl;
                cout << "got     : " << bitset<ell>(z_recon) << endl;
                cout << "x       : " << bitset<ell>(x_plain) << endl;
                cout << "y       : " << bitset<ell>(y_plain) << endl;
                cout << "b       : " << bitset<1>(b_plain) << endl;
                exit(1);
            }
        }
    }
    cout << "PI_select test passed!" << endl;
}