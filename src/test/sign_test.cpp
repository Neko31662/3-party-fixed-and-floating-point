#include "protocol/sign.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 63;
const ShareValue k = 676767676767;

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

    // 非向量测试
    {
        // test
        for (int i = 0; i < test_nums; i++) {
            ShareValue test_value = i;
            if (i % 3 == 1) {
                test_value = ((ShareValue(1) << ell) - i) & MSSshare<ell>::MASK;
            }
            if (i % 3 == 2) {
                private_PRG.gen_random_data(&test_value, sizeof(ShareValue));
                test_value &= MSSshare<ell>::MASK;
            }

            PI_sign_intermediate<ell, k> intermediate;
            MSSshare<ell> x_share;
            MSSshare_p b_share{k};

            // preprocess
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
            PI_sign_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &b_share);
            if (party_id == 0 || party_id == 1) {
                netio->send_stored_data(2);
            }
            // share
            MSSshare_share_from(0, party_id, *netio, &x_share, test_value);

            // compute
            PI_sign(party_id, PRGs, *netio, intermediate, &x_share, &b_share);

            // reveal
            ShareValue rec = MSSshare_p_recon(party_id, *netio, &b_share);
            ShareValue expected = test_value & (ShareValue(1) << (ell - 1)) ? 1 : 0;
            if (rec != expected && party_id == 0) {
                cout << "Test " << i << " failed! Expected: " << expected << ", got: " << rec
                     << endl;
                exit(1);
            }
        }
        cout << "PI_sign test passed!" << endl;
    }

    delete netio;
    return 0;
}