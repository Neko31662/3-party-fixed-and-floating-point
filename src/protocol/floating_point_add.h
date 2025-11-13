#pragma once
#include "config/config.h"
#include "protocol/align.h"
#include "protocol/eq.h"
#include "protocol/floating_point.h"
#include "protocol/great.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/select.h"
#include "protocol/shift.h"
#include "protocol/trunc.h"
#include "protocol/wrap.h"
#include "utils/misc.h"

struct PI_select_FLT_intermediate {
    PI_select_intermediate b_inter;
    PI_select_intermediate t_inter;
    PI_select_intermediate e_inter;
    PI_select_FLT_intermediate(int lf, int le) : b_inter(1), t_inter(lf + 2), e_inter(le) {}
};

struct PI_float_add_intermediate {
    int lf;
    int le;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    // input: x.e , y.e; output: 1 bit c1
    PI_great_intermediate great_inter1;
    // input: x.t , y.t; output: 1 bit c3
    PI_great_intermediate great_inter2;
    // input: zeta1 , lf; output: 1 bit d
    PI_great_intermediate great_inter3;
    // input: x.e , y.e; output: 1 bit c2
    PI_eq_intermediate eq_inter1;
    // 用于选择 {x,y} 和 {y,x}，得到 X 和 Y
    PI_select_FLT_intermediate select_FLT_inter1, select_FLT_inter2;
    // 用于检测 2+lf bit 的 Y.t 的分享的进位数，得到2+2lf bit的e
    PI_wrap2_spec_intermediate wrap_inter1;
    // 用于左移 2+2*lf bit 的 Y.t 的分享，使之对齐X.t，得到Y_t_prime1，2+2lf bits
    PI_shift_intermediate shift_inter1;
    // 根据 d ，在 0 和 Y.t 中选择，得到Y_t_prime2，2+lf bits
    PI_select_intermediate select_inter3;
    // 根据 g ，在 z2 和 z1 中选择，分别是相减和相加的结果，得到z_t_prime1，2+lf bits
    PI_select_intermediate select_inter4;
    // 将z_t_prime1左移到首位为1，得到z_t_prime2，2+lf bits和zeta，le bits
    PI_align_intermediate align_inter1;
    // 将z_t_prime2右移1位，得到最终的z.t，2+lf bits
    PI_trunc_intermediate trunc_inter1;

    MSSshare_p c1, c2, c3, d; // 1 bit

    MSSshare_mul_res c_tmp1, c_tmp2; // 1 bit c_tmp1= c2*c3 , c_tmp2= c1*c2
    MSSshare c;                      // 1 bit, c= c2*c3 + c1 - c1*c2

    FLTshare X, Y; // 被选择之后的浮点数

    MSSshare_p e;

    MSSshare_mul_res Y_t_prime1; // 左移对齐之后的Y.t
    MSSshare Y_t_prime2;         // 根据d选择之后的Y.t
    MSSshare z_t_prime1;         // 相加或相减之后的z.t
    MSSshare z_t_prime2;         // 左对齐之后的z.t
    MSSshare_mul_res zeta;       // 左对齐之后的zeta

    PI_float_add_intermediate(int lf, int le)
        : // great_inter
          great_inter1(le, 1 << 1), great_inter2(lf + 2, 1 << 1), great_inter3(le, 1 << 1),
          // eq_inter
          eq_inter1(le, 1 << 1),
          // select_FLT_inter
          select_FLT_inter1(lf, le), select_FLT_inter2(lf, le),
          // wrap_inter
          wrap_inter1(lf + 2, ShareValue(1) << (2 * lf + 2)),
          // shift_inter
          shift_inter1(2 + 2 * lf),
          // select_inter
          select_inter3(2 + lf), select_inter4(2 + lf),
          // align_inter
          align_inter1(lf + 2, le),
          // trunc_inter
          trunc_inter1(lf + 2),
          // c1,c2,c3,d
          c1(1 << 1), c2(1 << 1), c3(1 << 1), d(1 << 1),
          // c_tmp1,c_tmp2
          c_tmp1(1), c_tmp2(1),
          // c
          c(1),
          // X,Y
          X(lf, le), Y(lf, le),
          // e
          e(ShareValue(1) << (2 + 2 * lf)),
          // Y_t_prime1
          Y_t_prime1(2 + 2 * lf),
          // Y_t_prime2
          Y_t_prime2(2 + lf),
          // z_t_prime1
          z_t_prime1(2 + lf),
          // z_t_prime2
          z_t_prime2(2 + lf),
          // zeta
          zeta(le) {
        this->lf = lf;
        this->le = le;
    }
};

void PI_float_add_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                PRGSync private_PRG, PI_float_add_intermediate &intermediate,
                                FLTshare *input_x, FLTshare *input_y, FLTshare *output_z);

void PI_float_add(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     PI_float_add_intermediate &intermediate, FLTshare *input_x,
                     FLTshare *input_y, FLTshare *output_z);

#include "protocol/floating_point_add.tpp"