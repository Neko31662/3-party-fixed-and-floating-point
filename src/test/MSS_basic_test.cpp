#include "protocol/masked_RSS.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int ell = 64;

// circuit: (a+b) * (c*d) + (e*f) + g
uint64_t secret[10] = {111111111, 22222222222222, 33333333333333, 44444444444444,
                       555555555, 666666666,      777777777};

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
        netio->flush_all();
    } else if (party_id == 1) {
        netio->recv_data(0, &seed1, sizeof(block));
    } else if (party_id == 2) {
        netio->recv_data(0, &seed1, sizeof(block));
    }
    std::vector<std::shared_ptr<PRGSync>> PRGs(2);
    PRGs[0] = std::make_shared<PRGSync>(&seed1);
    PRGs[1] = std::make_shared<PRGSync>(&seed2);

    // sample shares
    int len = 20;
    set_MSSshare_party_id<ell>(party_id);
    std::vector<std::shared_ptr<MSSshare<ell>>> s(len);
    for (int i = 0; i < (int)s.size(); i++) {
        s[i] = std::make_shared<MSSshare<ell>>();
    }
    std::vector<std::shared_ptr<MSSshare_add_res<ell>>> s_add(len);
    for (int i = 0; i < (int)s_add.size(); i++) {
        s_add[i] = std::make_shared<MSSshare_add_res<ell>>();
    }
    std::vector<std::shared_ptr<MSSshare_mul_res<ell>>> s_mul(len);
    for (int i = 0; i < (int)s_mul.size(); i++) {
        s_mul[i] = std::make_shared<MSSshare_mul_res<ell>>();
    }

    // preprocess
    //    i: 0, 1, 2, 3, 4, 5, 6
    // s[i]: a, b, c, d, e, f, g
    for (int i = 0; i < (int)s.size(); i++) {
        s[i]->preprocess(i % 3, PRGs);
    }
    //        i: 0,   1,                     2
    // s_add[i]: a+b, (a+b) * (c*d) + (e*f), (a+b) * (c*d) + (e*f) + g
    // ======================================================================
    //        i: 0,   1,   2
    // s_mul[i]: c*d, e*f, (a+b) * (c*d)
    s_add[0]->preprocess(s[0], s[1]);
    s_mul[0]->preprocess(PRGs, *netio, s[2], s[3]);
    s_mul[1]->preprocess(PRGs, *netio, s[4], s[5]);
    s_mul[2]->preprocess(PRGs, *netio, s_mul[0], s_add[0]);
    s_add[1]->preprocess(s_mul[2], s_mul[1]);
    s_add[2]->preprocess(s_add[1], s[6]);

    // preprocess后需要调用这个
    if (party_id == 0) {
        netio->send_stored_data(2);
    }

    // share
    // 要按holder顺序发送，避免死锁
    for (int holder = 0; holder < 3; holder++) {
        for (int i = 0; i < 7; i++) {
            if (i % 3 == holder)
                s[i]->share_from(i % 3, secret[i], *netio);
        }
        if (party_id == holder) {
            netio->send_stored_data_all();
        }
    }

    // calculate
    s_add[0]->calc_add(); // a + b
    std::vector<std::shared_ptr<MSSshare_mul_res<ell> > > tmp_vec;
    tmp_vec.push_back(s_mul[0]);
    tmp_vec.push_back(s_mul[1]);
    calc_mul_vec(tmp_vec, *netio);
    // c * d, e * f
    // s_mul[0]->calc_mul(*netio); // c * d
    // s_mul[1]->calc_mul(*netio); // e * f
    s_mul[2]->calc_mul(*netio); // (a+b) * (c*d)
    s_add[1]->calc_add();       // (a+b) * (c*d) + (e*f)
    s_add[2]->calc_add();       // (a+b) * (c*d) + (e*f) + g

    // reconstruct
    ShareValue rec[len];
    ShareValue rec_add[len];
    ShareValue rec_mul[len];
    for (int i = 0; i <= 6; i++) {
        rec[i] = s[i]->recon(*netio);
    }
    for (int i = 0; i <= 2; i++) {
        rec_add[i] = s_add[i]->recon(*netio);
    }
    for (int i = 0; i <= 2; i++) {
        rec_mul[i] = s_mul[i]->recon(*netio);
    }
    delete netio;

    // show result
    for (int i = 0; i <= 6; i++) {
        cout << "Reconstructed s[" << i << "]: " << rec[i] << endl;
    }
    for (int i = 0; i <= 2; i++) {
        cout << "Reconstructed s_add[" << i << "]: " << rec_add[i] << endl;
    }
    for (int i = 0; i <= 2; i++) {
        cout << "Reconstructed s_mul[" << i << "]: " << rec_mul[i] << endl;
    }
    ShareValue res =
        ((secret[0] + secret[1]) * (secret[2] * secret[3]) + (secret[4] * secret[5]) + secret[6]);
    if (ell < 64)
        res %= (1ULL << ell);
    cout << "Reconstructed result: " << rec_add[2] << endl;
    cout << "Expected result: " << res << endl;
    if (rec_add[2] == res) {
        cout << "MSS basic test passed!" << endl;
        return 0;
    } else {
        cout << "MSS basic test failed!" << endl;
        return 1;
    }
}