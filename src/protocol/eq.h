#pragma once
#include "config/config.h"
#include "protocol/addictive_share_p.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "utils/misc.h"

struct PI_eq_intermediate {
    ShareValue k;
    ShareValue p;
    std::vector<ADDshare_p> t_list, rd_list;
    ADDshare_p sigma{p};

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_eq_intermediate(int ell, ShareValue k) : p(nxt_prime(ell)), sigma(p) {
        this->k = k;
        t_list = std::vector<ADDshare_p>(p, ADDshare_p{k});
        rd_list = std::vector<ADDshare_p>(ell, ADDshare_p{p});
    }
};

void PI_eq_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                      PRGSync private_PRG, PI_eq_intermediate &intermediate,
                      MSSshare *input_x, MSSshare *input_y, MSSshare_p *output_b);

void PI_eq(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
           PI_eq_intermediate &intermediate, MSSshare *input_x, MSSshare *input_y,
           MSSshare_p *output_b);

#include "protocol/eq.tpp"