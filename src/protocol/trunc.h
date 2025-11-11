#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "protocol/wrap.h"
#include "utils/misc.h"

template <int ell> struct PI_trunc_intermediate {
    PI_wrap2_intermediate<ell, (ShareValue(1) << ell)> wrap2_intermediate;
    MSSshare_p_add_res wrap2_res{ShareValue(1) << ell};
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    int input_bits;
#endif
};

template <int ell>
void PI_trunc_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_trunc_intermediate<ell> &intermediate, MSSshare<ell> *input_x,
                         int input_bits, MSSshare_add_res<ell> *output_z);

template <int ell>
void PI_trunc(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_trunc_intermediate<ell> &intermediate, MSSshare<ell> *input_x, int input_bits,
              MSSshare_add_res<ell> *output_z);

#include "protocol/trunc.tpp"