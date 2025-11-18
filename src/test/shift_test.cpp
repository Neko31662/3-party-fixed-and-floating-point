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

    // 非向量
    for (int k = 0; k < ell; ++k) {
        ShareValue plain_x = 24678;

        PI_shift_intermediate intermediate(ell);
        ADDshare<> k_share(LOG_1(ell));
        MSSshare x_share(ell);
        MSSshare_mul_res output_res(ell);

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
    cout << "PI_shift test passed!" << endl;

    // 向量
    for (int k = 0; k < ell; ++k) {
        int vec_len = 10;
        ShareValue plain_x = 24678;

        vector<PI_shift_intermediate> intermediate(vec_len, ell);
        vector<ADDshare<>> k_share(vec_len, LOG_1(ell));
        vector<MSSshare> x_share(vec_len, ell);
        vector<MSSshare_mul_res> output_res(vec_len, ell);

        // preprocess
        for (int i = 0; i < vec_len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share[i]);
            PI_shift_preprocess(party_id, PRGs, *netio, intermediate[i], &x_share[i],
                                &output_res[i]);
        }
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        // share
        ShareValue k_value = k;
        for (int i = 0; i < vec_len; i++) {
            ADDshare_share_from(0, party_id, PRGs, *netio, &k_share[i], k_value);
            MSSshare_share_from(0, party_id, *netio, &x_share[i], plain_x);
        }

        // compute
        auto intermediate_ptr = make_ptr_vec(intermediate);
        auto x_share_ptr = make_ptr_vec(x_share);
        auto k_share_ptr = make_ptr_vec(k_share);
        auto output_res_ptr = make_ptr_vec(output_res);
        PI_shift_vec(party_id, PRGs, *netio, intermediate_ptr, x_share_ptr, k_share_ptr,
                     output_res_ptr);

        // reveal
        ShareValue result;
        for (int i = 0; i < vec_len; i++) {
            result = MSSshare_recon(party_id, *netio, &output_res[i]);
            cout << "k = " << k << ", result = " << result << endl;
            if (result != ((plain_x << k) & output_res[i].MASK)) {
                cout << "Error in PI_shift_vec!" << endl;
                cout << "Expected: " << ((plain_x << k) & output_res[i].MASK) << endl;
                cout << "Actual: " << result << endl;
                exit(-1);
            }
        }
    }
    cout << "PI_shift_vec test passed!" << endl;
}