#pragma once
#include "config/config.h"
#include "protocol/addictive_share_p.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "utils/misc.h"

template <int ell, ShareValue k> struct PI_eq_intermediate {
    constexpr static ShareValue p = nxt_prime(ell);
    std::vector<ADDshare_p> t_list, rd_list;
    ADDshare_p sigma{p};

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_eq_intermediate() : t_list(p, ADDshare_p{k}), rd_list(ell, ADDshare_p{p}) {}
};

template <int ell, ShareValue k>
void PI_eq_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                      PRGSync private_PRG, PI_eq_intermediate<ell, k> &intermediate,
                      MSSshare<ell> *input_x, MSSshare<ell> *input_y, MSSshare_p *output_b);

template <int ell, ShareValue k>
void PI_eq(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
           PI_eq_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x, MSSshare<ell> *input_y,
           MSSshare_p *output_b);

#include "protocol/eq.tpp"