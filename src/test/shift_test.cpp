#include "protocol/shift.h"

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int ell = 32;

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

    for (int k = 0; k < ell; ++k) {
        ShareValue plain_x = 24678;

        PI_shift_intermediate<ell> intermediate;
        ADDshare<LOG_1(ell)> k_share;
        MSSshare<ell> x_share;
        MSSshare_mul_res<ell> output_res;

        // preprocess
        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        PI_shift_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &output_res);
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue k_value = k;
        ADDshare_share_from(0, party_id, PRGs, *netio, &k_share, k_value);
        MSSshare_share_from(0, party_id, *netio, &x_share, plain_x);

        // compute
        PI_shift(party_id, PRGs, *netio, intermediate, &x_share, &k_share, &output_res);

        // reveal
        ShareValue result;
        result = MSSshare_recon(party_id, *netio, &output_res);
        cout << "k = " << k << ", result = " << result << endl;
        if (result != ((plain_x << k) & output_res.MASK)) {
            cout << "Error in PI_shift!" << endl;
            cout << "Expected: " << ((plain_x << k) & output_res.MASK) << endl;
            cout << "Actual: " << result << endl;
            exit(-1);
        }
    }
}