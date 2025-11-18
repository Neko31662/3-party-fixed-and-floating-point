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

struct PI_align_intermediate {
    int ell;
    int ell2;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
    ShareValue p;
    ShareValue p2;
    std::vector<ADDshare_p> rx_list;
    std::vector<ADDshare_p> L1_list;
    std::vector<ADDshare_p> L2_list;
    std::vector<ADDshare_p> D_list;
    ADDshare_p zeta1_addp;
    MSSshare_p c_mssp, zeta1_mssp;
    MSSshare_p b_mssp;
    MSSshare_mul_res xprime_mss;

    MSSshare_mul_res tmp2; //(c * (xprime - xprime2) )

    PI_wrap1_intermediate wrap1_intermediate;
    PI_shift_intermediate shift_intermediate;
    PI_sign_intermediate sign_intermediate;
    PI_convert_intermediate convert_intermediate;

    MSSshare_mul_res temp_z2_mss; // (temp_z_mss - x)(1-signx)
    MSSshare temp_zeta_mss;       // 原本的输出zeta
    MSSshare_p signx_mssp;        // signx
    PI_sign_intermediate sign_intermediate2;

    PI_align_intermediate(int ell, int ell2)
        : p(nxt_prime(ell)), p2(ShareValue(1) << LOG_1(ell)), rx_list(ell, p), L1_list(ell, p),
          L2_list(ell, p), D_list(ell, p), b_mssp(p), xprime_mss(ell), tmp2(ell),
          wrap1_intermediate(ell, p), shift_intermediate(ell),
          sign_intermediate(ell, (ShareValue(1) << (ell))),
          convert_intermediate((ShareValue(1) << (ell2))), temp_z2_mss(ell), temp_zeta_mss(ell2),
          signx_mssp((ShareValue(1) << ell)), sign_intermediate2(ell, (ShareValue(1) << (ell))),
          zeta1_addp(p2), c_mssp(ShareValue(1) << ell), zeta1_mssp(ShareValue(1) << ell2) {
        this->ell = ell;
        this->ell2 = ell2;
        if (ell2 > ell) {
            error("PI_align_intermediate: ell2 must be less or equal to ell");
        }
    }
};

inline void PI_align_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                PI_align_intermediate &intermediate, MSSshare *input_x,
                                MSSshare *output_z, MSSshare_mul_res *output_zeta);

inline void PI_align(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     PI_align_intermediate &intermediate, MSSshare *input_x, MSSshare *output_z,
                     MSSshare_mul_res *output_zeta);

inline void PI_align_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         std::vector<PI_align_intermediate *> &intermediate_vec,
                         std::vector<MSSshare *> &input_x_vec,
                         std::vector<MSSshare *> &output_z_vec,
                         std::vector<MSSshare_mul_res *> &output_zeta_vec);

#include "protocol/align.tpp"