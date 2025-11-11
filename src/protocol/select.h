#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"

template <int ell> struct PI_select_intermediate {
    MSSshare<ell> rb;
    MSSshare_mul_res<ell> mb_mul_rb;
    MSSshare_mul_res<ell> bprime_mul_dif;

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
};

template <int ell>
void PI_select_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                          PI_select_intermediate<ell> &intermediate, MSSshare<ell> *input_x,
                          MSSshare<ell> *input_y, MSSshare<1> *input_b,
                          MSSshare_add_res<ell> *output_z);

template <int ell>
void PI_select(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               PI_select_intermediate<ell> &intermediate, MSSshare<ell> *input_x,
               MSSshare<ell> *input_y, MSSshare<1> *input_b, MSSshare_add_res<ell> *output_z);

#include "protocol/select.tpp"