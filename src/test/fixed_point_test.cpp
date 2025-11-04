#include "protocol/fixed-point.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int l = 32;
const int lf = 16;

uint64_t secret[10] = {11111, 22222, 33333, 44444, 55555, 66666, 77777};

int main(int argc, char **argv) {
    // net io
    int party_id = atoi(argv[1]);
    NetIOMP *netio = new NetIOMP(party_id, BASE_PORT, NUM_PARTIES);
    netio->sync();
    log("Phase 1");

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
    log("Phase 2");

    // shares allocation
    int len = 20;
    set_fixed_point_share_party_id<l, lf>(party_id);
    std::vector<std::shared_ptr<fixed_point_share<l, lf>>> s(len);
    for (int i = 0; i < (int)s.size(); i++) {
        s[i] = std::make_shared<fixed_point_share<l, lf>>();
    }
    std::vector<std::shared_ptr<fixed_point_share_add_res<l, lf>>> s_add(len);
    for (int i = 0; i < (int)s.size(); i++) {
        s_add[i] = std::make_shared<fixed_point_share_add_res<l, lf>>();
    }
    log("Phase 3");

    // preprocess
    //    i: 0, 1, 2, 3, 4, 5, 6
    // s[i]: a, b, c, d, e, f, g
    for (int i = 0; i < (int)s.size(); i++) {
        s[i]->preprocess(i % 3, PRGs);
    }
    s_add[0]->preprocess(s[0], s[1]);
    log("Phase 3.2");

    // preprocess后需要调用这个
    if (party_id == 0) {
        netio->send_stored_data(2);
    }
    log("Phase 4");

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

    // reconstruct
    ShareValue rec[len];
    ShareValue rec_add[len];
    ShareValue rec_mul[len];
    for (int i = 0; i <= 6; i++) {
        rec[i] = s[i]->recon(*netio);
    }
    for (int i = 0; i <= 0; i++) {
        rec_add[i] = s_add[i]->recon(*netio);
    }
    // for (int i = 0; i <= 2; i++) {
    //     rec_mul[i] = s_mul[i]->recon(*netio);
    // }
    delete netio;

    // show result
    for (int i = 0; i <= 6; i++) {
        cout << "Reconstructed s[" << i << "]: " << (uint64_t)rec[i] << endl;
    }
    for (int i = 0; i <= 0; i++) {
        cout << "Reconstructed s_add[" << i << "]: " << (uint64_t)rec_add[i] << endl;
    }
    // for (int i = 0; i <= 2; i++) {
    //     cout << "Reconstructed s_mul[" << i << "]: " << rec_mul[i] << endl;
    // }
    ShareValue res = (secret[0] + secret[1]);
    cout << "Reconstructed result: " << (uint64_t)rec_add[0] << endl;
    cout << "Expected result: " << (uint64_t)res << endl;
    if (rec_add[0] == res) {
        cout << "Fixed point test passed!" << endl;
        return 0;
    } else {
        cout << "Fixed point test failed!" << endl;
        return 1;
    }
}