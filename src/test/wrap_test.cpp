#include "protocol/wrap.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int ell = 63;
const ShareValue k = 67676767;
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
    auto private_seed = gen_seed();
    auto private_PRG = PRGSync(&private_seed);

    // spec
    for (int test_i = 0; test_i < test_nums; ++test_i) {
        ShareValue plain_x;
        private_PRG.gen_random_data(&plain_x, sizeof(ShareValue));
        plain_x = plain_x & ((ShareValue(1) << (ell - 1)) - 1);
        MSSshare<ell> x_share;
        PI_wrap1_spec_intermediate<ell, k> intermediate1;
        PI_wrap2_spec_intermediate<ell, k> intermediate2;
        MSSshare_p z1_share{k};
        MSSshare_p z2_share{k};

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        PI_wrap1_spec_preprocess(party_id, PRGs, *netio, intermediate1, &x_share, &z1_share);
        PI_wrap2_spec_preprocess(party_id, PRGs, *netio, intermediate2, &x_share, &z2_share);

        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        MSSshare_share_from(0, party_id, *netio, &x_share, plain_x);

        // compute
        PI_wrap1_spec(party_id, PRGs, *netio, intermediate1, &x_share, &z1_share);
        PI_wrap2_spec(party_id, PRGs, *netio, intermediate2, &x_share, &z2_share);

        // recon
        ShareValue z1 = MSSshare_p_recon(party_id, *netio, &z1_share);
        ShareValue z2 = MSSshare_p_recon(party_id, *netio, &z2_share);

        ShareValue mx;
        if (party_id == 1) {
            ShareValue v = x_share.v1;
            netio->send_data(0, &v, sizeof(ShareValue));
        } else if (party_id == 0) {
            netio->recv_data(1, &mx, sizeof(ShareValue));
        }

        if (party_id == 0) {
            int rb;
            ShareValue r = x_share.v1 + x_share.v2;
            if (ell == ShareValue_BitLength) {
                rb = (r < x_share.v1);
            } else {
                rb = (r >> ell);
            }
            int mb;
            ShareValue x = (r & x_share.MASK) + mx;
            if (ell == ShareValue_BitLength) {
                mb = (x < mx);
            } else {
                mb = (x >> ell);
            }
            ShareValue expected_z1 = mb;
            ShareValue expected_z2 = mb + rb;
            // cout << "r1=" << bitset<ell>(x_share.v1) << endl;
            // cout << "r2=" << bitset<ell>(x_share.v2) << endl;
            // cout << "mx=" << bitset<ell>(mx) << endl;
            // cout << "rb=" << rb << endl;
            // cout << "mb=" << mb << endl;

            if (z1 != expected_z1) {
                cout << "PI_wrap1_spec failed at index " << test_i << ": expected " << expected_z1
                     << ", got " << z1 << endl;
                exit(1);
            }
            if (z2 != expected_z2) {
                cout << "PI_wrap2_spec failed at index " << test_i << ": expected " << expected_z2
                     << ", got " << z2 << endl;
                exit(1);
            }
        }
    }
    cout << "PI_wrap1_spec and PI_wrap2_spec passed! " << endl;

    // normal
    for (int test_i = 0; test_i < test_nums; ++test_i) {
        ShareValue plain_x;
        private_PRG.gen_random_data(&plain_x, sizeof(ShareValue));
        plain_x = plain_x & ((ShareValue(1) << (ell)) - 1);
        MSSshare<ell> x_share;
        PI_wrap1_intermediate<ell, k> intermediate1;
        PI_wrap2_intermediate<ell, k> intermediate2;
        MSSshare_p_add_res z1_share{k};
        MSSshare_p_add_res z2_share{k};

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        PI_wrap1_preprocess(party_id, PRGs, *netio, intermediate1, &x_share, &z1_share);
        PI_wrap2_preprocess(party_id, PRGs, *netio, intermediate2, &x_share, &z2_share);

        if (party_id == 0 || party_id == 1) {
            netio->send_stored_data(2);
        }

        // share
        MSSshare_share_from(0, party_id, *netio, &x_share, plain_x);

        // compute
        PI_wrap1(party_id, PRGs, *netio, intermediate1, &x_share, &z1_share);
        PI_wrap2(party_id, PRGs, *netio, intermediate2, &x_share, &z2_share);

        // recon
        ShareValue z1 = MSSshare_p_recon(party_id, *netio, &z1_share);
        ShareValue z2 = MSSshare_p_recon(party_id, *netio, &z2_share);

        ShareValue mx;
        if (party_id == 1) {
            ShareValue v = x_share.v1;
            netio->send_data(0, &v, sizeof(ShareValue));
        } else if (party_id == 0) {
            netio->recv_data(1, &mx, sizeof(ShareValue));
        }

        if (party_id == 0) {
            int rb;
            ShareValue r = x_share.v1 + x_share.v2;
            if (ell == ShareValue_BitLength) {
                rb = (r < x_share.v1);
            } else {
                rb = (r >> ell);
            }
            int mb;
            ShareValue x = (r & x_share.MASK) + mx;
            if (ell == ShareValue_BitLength) {
                mb = (x < mx);
            } else {
                mb = (x >> ell);
            }
            ShareValue expected_z1 = mb;
            ShareValue expected_z2 = mb + rb;
            // cout << "r1=" << bitset<ell>(x_share.v1) << endl;
            // cout << "r2=" << bitset<ell>(x_share.v2) << endl;
            // cout << "mx=" << bitset<ell>(mx) << endl;
            // cout << "rb=" << rb << endl;
            // cout << "mb=" << mb << endl;

            if (z1 != expected_z1) {
                cout << "PI_wrap1 failed at index " << test_i << ": expected " << expected_z1
                     << ", got " << z1 << endl;
                exit(1);
            }
            if (z2 != expected_z2) {
                cout << "PI_wrap2 failed at index " << test_i << ": expected " << expected_z2
                     << ", got " << z2 << endl;
                exit(1);
            }
        }
    }
    cout << "PI_wrap1 and PI_wrap2 passed! " << endl;
}