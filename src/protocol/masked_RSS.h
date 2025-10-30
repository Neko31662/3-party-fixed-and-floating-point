#pragma once

#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

template <int ell> class MSSshare {
  public:
    // For party 0, v1: r1, v2: r2
    // For party 1, v1: m , v2: r1
    // For party 2, v1: m , v2: r2
    uint64_t v1, v2;

    MSSshare() {
        if (ell > 64) {
            error("MSSshare only supports ell <= 64 now");
        }
    }

    // P0.PRGs[0] <----sync----> P1.PRGs[0]
    // P0.PRGs[1] <----sync----> P2.PRGs[0]
    void preprocess(int party_id, PRGSync *PRGs) {
        int bytelen = (ell + 7) / 8;
        if (party_id == 0) {
            PRGs[0].gen_random_data(&v1, bytelen);
            PRGs[1].gen_random_data(&v2, bytelen);
        } else if (party_id == 1) {
            PRGs[0].gen_random_data(&v2, bytelen);
        }else if (party_id == 2) {
            PRGs[0].gen_random_data(&v2, bytelen);
        }
    }
};