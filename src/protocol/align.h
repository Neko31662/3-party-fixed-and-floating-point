#pragma once
#include "config/config.h"
#include "protocol/addictive_share.h"
#include "protocol/addictive_share_p.h"
#include "protocol/convert.h"
#include "protocol/detect.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/shift.h"
#include "protocol/sign.h"
#include "protocol/wrap.h"
#include "utils/misc.h"
#include "utils/permutation.h"

template <int ell, int ell2> struct PI_align_intermediate {
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
    constexpr static ShareValue p = nxt_prime(ell);
    constexpr static ShareValue p2 = (ShareValue(1) << LOG_1(ell));
    std::vector<ADDshare_p> rx_list;
    std::vector<ADDshare_p> L1_list;
    std::vector<ADDshare_p> L2_list;
    std::vector<ADDshare_p> D_list;
    ADDshare_p zeta1_addp{p2};
    ADDshare<LOG_1(ell)> zeta1_add;
    MSSshare_p c_mssp{(ShareValue(1) << ell)}, zeta1_mssp{(ShareValue(1) << ell2)};
    MSSshare<ell> c_mss;
    MSSshare<ell2> c_mss2, zeta1_mss;
    MSSshare_add_res<ell> xprime2_mss;
    MSSshare_p_add_res b_mssp{p};
    MSSshare_mul_res<ell> xprime_mss;

    MSSshare_add_res<ell> tmp1; //(xprime - xprime2)
    MSSshare_mul_res<ell> tmp2; //(c * (xprime - xprime2) )

    PI_wrap1_intermediate<ell, p> wrap1_intermediate;
    PI_shift_intermediate<ell> shift_intermediate;
    PI_sign_intermediate<ell, (ShareValue(1) << (ell))> sign_intermediate;
    PI_convert_intermediate<(ShareValue(1) << (ell2))> convert_intermediate;

    MSSshare_add_res<ell> temp_z_mss;              // 原本的输出z
    MSSshare_mul_res<ell> temp_z2_mss;             // (temp_z_mss - x)(1-signx)
    MSSshare_add_res<ell2> temp_zeta_mss;          // 原本的输出zeta
    MSSshare_p signx_mssp{(ShareValue(1) << ell)}; // signx
    PI_sign_intermediate<ell, (ShareValue(1) << (ell))> sign_intermediate2;

    PI_align_intermediate() : rx_list(ell, p), L1_list(ell, p), L2_list(ell, p), D_list(ell, p) {
        if (ell2 > ell) {
            error("PI_align_intermediate: ell2 must be less or equal to ell");
        }
    }
};

template <int ell, int ell2>
inline void PI_align_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                PI_align_intermediate<ell, ell2> &intermediate,
                                MSSshare<ell> *input_x, MSSshare_add_res<ell> *output_z,
                                MSSshare_mul_res<ell2> *output_zeta);

template <int ell, int ell2>
inline void PI_align(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     PI_align_intermediate<ell, ell2> &intermediate, MSSshare<ell> *input_x,
                     MSSshare_add_res<ell> *output_z, MSSshare_mul_res<ell2> *output_zeta);

#include "protocol/align.tpp"