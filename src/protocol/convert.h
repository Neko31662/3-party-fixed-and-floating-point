#pragma once
#include "config/config.h"
#include "protocol/addictive_share.h"
#include "protocol/addictive_share_p.h"
#include "protocol/masked_RSS_p.h"
#include "utils/misc.h"

struct PI_convert_intermediate {
    ADDshare_p_mul_res c;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_convert_intermediate(ShareValue k) : c(k) {}
};

void PI_convert_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                           PI_convert_intermediate &intermediate, ADDshare<> *input_x,
                           MSSshare_p *output_z);

/*将2^ell上的ADDshare分享转换为模k的MSSshare_p分享，要求input_x对应的明文最高位是0
 */
void PI_convert(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                PI_convert_intermediate &intermediate, ADDshare<> *input_x, MSSshare_p *output_z);

void PI_convert_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                    std::vector<PI_convert_intermediate *> &intermediate_vec,
                    std::vector<ADDshare<> *> &input_x_vec,
                    std::vector<MSSshare_p *> &output_z_vec);
#include "protocol/convert.tpp"