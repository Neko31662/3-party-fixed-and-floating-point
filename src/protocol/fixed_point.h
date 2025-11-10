#pragma once

#include "config/config.h"
#include "protocol/addictive_share.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

// li: 乘数的整数位长度， lf: 乘数和结果的小数位长度， l_res: 结果的总长度

template <int li, int lf, int l_res> struct PI_fixed_mult_intermediate {
    static const int l_input = li + lf;

    ADDshare<lf + l_res> Gamma;
    ADDshare<lf + l_res - l_input> lf_share1[2]; // r_x(0)
    ADDshare<lf + l_res - l_input> lf_share2[2]; // w_x(0)+r_x(0)
    ADDshare<lf + l_res - l_input> lf_share3[2]; // (1-r_x(0))(r_y_1+r_y_2)

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_fixed_mult_intermediate() {
        if (li <= 0 || lf <= 0 || l_res <= 0) {
            error("PI_fixed_mult_intermediate: li,lf and l_res must be positive integers");
        }
        if (l_res < li + lf || l_res > 2 * li + lf) {
            error("PI_fixed_mult_intermediate: l_res must be between (li + lf) and (2 * li + lf)");
        }
        if (li + lf < 2) {
            error("PI_fixed_mult_intermediate: li + lf must be at least 2");
        }
    }
};

template <int li, int lf, int l_res>
void PI_fixed_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_fixed_mult_intermediate<li, lf, l_res> &PI_fixed_mult_intermediate,
                              MSSshare<li + lf> *input_x, MSSshare<li + lf> *input_y,
                              MSSshare<l_res> *output_z);

template <int li, int lf, int l_res>
void PI_fixed_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_fixed_mult_intermediate<li, lf, l_res> &PI_fixed_mult_intermediate,
                   MSSshare<li + lf> *input_x, MSSshare<li + lf> *input_y,
                   MSSshare<l_res> *output_z);

#include "protocol/fixed_point.tpp"
