#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/sign.h"
#include "utils/misc.h"
#include "utils/permutation.h"

template <int ell, ShareValue k> struct PI_great_intermediate {
    std::vector<PI_sign_intermediate<ell, k>> sign_intermediate;
    std::vector<MSSshare_p> sign_res; // sx, sy, sd=sign(y-x)

    MSSshare_p_mul_res tmp1{k}; // sx*sy
    MSSshare_p_mul_res tmp2{k}; // (1-sx-sy+2sx*sy)*sd
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_great_intermediate() : sign_intermediate(3), sign_res(3, MSSshare_p{k}) {}
};

template <int ell, ShareValue k>
void PI_great_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_great_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
                         MSSshare<ell> *input_y, MSSshare_p_add_res *output_b);

template <int ell, ShareValue k>
void PI_great(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_great_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
              MSSshare<ell> *input_y, MSSshare_p_add_res *output_b);

#include "protocol/great.tpp"