#include "protocol/eq.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 64;
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
        PI_eq_intermediate<ell, k> intermediate;
        MSSshare<ell> x_share, y_share;
        MSSshare_p b_share{k};

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(1, party_id, PRGs, *netio, &y_share);
        PI_eq_preprocess(party_id, PRGs, *netio, private_PRG, intermediate, &x_share, &y_share,
                         &b_share);
        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue test_value_x = 0;
        ShareValue test_value_y = 0;
        if (party_id == 0) {
            private_PRG.gen_random_data(&test_value_x, sizeof(ShareValue));
            test_value_x &= MSSshare<ell>::MASK;
            private_PRG.gen_random_data(&test_value_y, sizeof(ShareValue));
            test_value_y &= MSSshare<ell>::MASK;
            if (test_i % 2 == 0) {
                test_value_y = test_value_x;
            }
        }
        MSSshare_share_from(0, party_id, *netio, &x_share, test_value_x);
        MSSshare_share_from(0, party_id, *netio, &y_share, test_value_y);

        // compute
        PI_eq(party_id, PRGs, *netio, intermediate, &x_share, &y_share, &b_share);

        // reveal
        ShareValue rec = MSSshare_p_recon(party_id, *netio, &b_share);
        ShareValue expected = (test_value_x == test_value_y) ? 1 : 0;
        if (rec != expected && party_id == 0) {
            cout << "Test " << test_i << " failed! Expected: " << expected << ", got: " << rec
                 << endl;
            cout << "x: " << bitset<ell>(test_value_x) << endl;
            cout << "y: " << bitset<ell>(test_value_y) << endl;
            exit(1);
        }
    }

    cout<<"PI_eq test passed!" << endl;
    delete netio;
    return 0;
}