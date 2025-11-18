#pragma once

#include "config/config.h"
#include "protocol/addictive_share.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

// li: 乘数的整数位长度， lf: 乘数和结果的小数位长度， l_res: 结果的总长度

struct PI_fixed_mult_intermediate {
    int li, lf, l_res;
    int l_input;

    ADDshare<> Gamma;
    ADDshare<> lf_share1[2]; // r_x(0)
    ADDshare<> lf_share2[2]; // w_x(0)+r_x(0)
    ADDshare<> lf_share3[2]; // (1-r_x(0))(r_y_1+r_y_2)

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_fixed_mult_intermediate(int li, int lf, int l_res)
        : Gamma(lf + l_res),
          lf_share1{ADDshare(lf + l_res - (li + lf)), ADDshare(lf + l_res - (li + lf))},
          lf_share2{ADDshare(lf + l_res - (li + lf)), ADDshare(lf + l_res - (li + lf))},
          lf_share3{ADDshare(lf + l_res - (li + lf)), ADDshare(lf + l_res - (li + lf))} {
        this->li = li;
        this->lf = lf;
        this->l_res = l_res;
        l_input = li + lf;
#ifdef DEBUG_MODE
        if (li <= 0 || lf <= 0 || l_res <= 0) {
            error("PI_fixed_mult_intermediate: li,lf and l_res must be positive integers");
        }
        if (l_res < li + lf || l_res > 2 * li + lf) {
            error("PI_fixed_mult_intermediate: l_res must be between (li + lf) and (2 * li + lf)");
        }
        if (li + lf < 2) {
            error("PI_fixed_mult_intermediate: li + lf must be at least 2");
        }
#endif
    }
};

void PI_fixed_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_fixed_mult_intermediate &PI_fixed_mult_intermediate,
                              MSSshare *input_x, MSSshare *input_y, MSSshare *output_z);

void PI_fixed_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_fixed_mult_intermediate &PI_fixed_mult_intermediate, MSSshare *input_x,
                   MSSshare *input_y, MSSshare *output_z, bool has_offset = true);

void PI_fixed_mult_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                       std::vector<PI_fixed_mult_intermediate *> &PI_fixed_mult_intermediate_vec,
                       std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare *> &input_y_vec,
                       std::vector<MSSshare *> &output_z_vec, bool has_offset = true);

#include "protocol/fixed_point.tpp"
