#pragma once
#include "config/config.h"
#include "protocol/addictive_share.h"
#include "protocol/addictive_share_p.h"
#include "protocol/masked_RSS_p.h"
#include "utils/misc.h"

template <ShareValue k> struct PI_convert_intermediate {
    ADDshare_p_mul_res c{k};
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
};

template <int ell, ShareValue k>
void PI_convert_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                           PI_convert_intermediate<k> &intermediate, ADDshare<ell> *input_x,
                           MSSshare_p *output_z);

/*将2^ell上的ADDshare分享转换为模k的MSSshare_p分享，要求input_x对应的明文最高位是0
 */
template <int ell, ShareValue k>
void PI_convert(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                PI_convert_intermediate<k> &intermediate, ADDshare<ell> *input_x,
                MSSshare_p *output_z);

#include "protocol/convert.tpp"