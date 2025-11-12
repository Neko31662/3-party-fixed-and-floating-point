#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/sign.h"
#include "utils/misc.h"
#include "utils/permutation.h"

struct PI_great_intermediate {
    int ell;
    ShareValue k;
    std::vector<PI_sign_intermediate> sign_intermediate;
    std::vector<MSSshare_p> sign_res; // sx, sy, sd=sign(y-x)

    MSSshare_p_mul_res tmp1; // sx*sy
    MSSshare_p_mul_res tmp2; // (1-sx-sy+2sx*sy)*sd
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_great_intermediate(int ell, ShareValue k) : tmp1(k), tmp2(k) {
        this->ell = ell;
        this->k = k;
        sign_intermediate = std::vector<PI_sign_intermediate>(3, PI_sign_intermediate(ell, k));
        sign_res = std::vector<MSSshare_p>(3, MSSshare_p{k});
    }
};

void PI_great_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_great_intermediate &intermediate, MSSshare *input_x,
                         MSSshare *input_y, MSSshare_p *output_b);

void PI_great(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_great_intermediate &intermediate, MSSshare *input_x,
              MSSshare *input_y, MSSshare_p *output_b);

#include "protocol/great.tpp"