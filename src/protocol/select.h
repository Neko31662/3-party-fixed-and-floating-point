#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"

struct PI_select_intermediate {
    int ell;
    MSSshare rb;
    MSSshare_mul_res mb_mul_rb;
    MSSshare_mul_res bprime_mul_dif;

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_select_intermediate(int ell) : rb(ell), mb_mul_rb(ell), bprime_mul_dif(ell) {
        this->ell = ell;
    }
};

void PI_select_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                          PI_select_intermediate &intermediate, MSSshare *input_x,
                          MSSshare *input_y, MSSshare *input_b, MSSshare *output_z);

void PI_select(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               PI_select_intermediate &intermediate, MSSshare *input_x, MSSshare *input_y,
               MSSshare *input_b, MSSshare *output_z);

void PI_select_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   std::vector<PI_select_intermediate *> &intermediate_vec,
                   std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare *> &input_y_vec,
                   std::vector<MSSshare *> &input_b_vec, std::vector<MSSshare *> &output_z_vec);

#include "protocol/select.tpp"