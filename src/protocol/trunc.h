#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "protocol/wrap.h"
#include "utils/misc.h"

struct PI_trunc_intermediate {
    int ell;
    PI_wrap2_intermediate wrap2_intermediate;
    MSSshare_p wrap2_res;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    int input_bits;
#endif
    PI_trunc_intermediate(int ell)
        : wrap2_intermediate(ell, ShareValue(1) << ell), wrap2_res(ShareValue(1) << ell) {
        this->ell = ell;
    }
};

void PI_trunc_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_trunc_intermediate &intermediate, MSSshare *input_x, int input_bits,
                         MSSshare *output_z);

void PI_trunc(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_trunc_intermediate &intermediate, MSSshare *input_x, int input_bits,
              MSSshare *output_z);

#include "protocol/trunc.tpp"