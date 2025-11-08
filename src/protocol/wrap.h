#pragma once
#include "config/config.h"
#include "protocol/addictive_share.h"
#include "protocol/addictive_share_p.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/sign.h"
#include "utils/misc.h"

// 命名规则：wrap1表示只考虑m+r的进位，wrap2表示还要考虑r1+r2的进位
// wrap1/2_spec表示确定输入的x首bit为0时的wrap协议

template <int ell, ShareValue k> struct PI_wrap1_spec_intermediate {
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
    ADDshare_p rx0{k};
};

template <int ell, ShareValue k> struct PI_wrap2_spec_intermediate {
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
    PI_wrap1_spec_intermediate<ell, k> wrap1_intermediate;
    ADDshare_p b{k};
};

template <int ell, ShareValue k> struct PI_wrap1_intermediate {
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
    MSSshare<ell + 1> tmpx;
    PI_sign_intermediate<ell + 1, k> sign_intermediate;
    MSSshare_p b{k};
    MSSshare_p c{k};
    MSSshare_p_mul_res b_mul_c{k};
};

template <int ell, ShareValue k> struct PI_wrap2_intermediate {
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
    PI_wrap1_intermediate<ell, k> wrap1_intermediate;
    MSSshare_p_add_res d{k}; // d is the output of wrap1
};

/* 预处理PI_wrap1_spec_intermediate对象和协议的输出
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param intermediate: 预处理的PI_wrap1_spec_intermediate对象引用
 * @param input_x: 作为协议输入的MSSshare对象指针，需要已经预处理完成
 * @param output_z: 协议输出的对象的指针
 *
 * @templateparam ell: 输入分享的位数，输入分享必须在2^ell上
 * @templateparam k: 输出分享的模数
 */
template <int ell, ShareValue k>
void PI_wrap1_spec_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_wrap1_spec_intermediate<ell, k> &intermediate,
                              MSSshare<ell> *input_x, MSSshare_p *output_z);

/* 与PI_wrap1_spec_preprocess类似
 */
template <int ell, ShareValue k>
void PI_wrap2_spec_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_wrap2_spec_intermediate<ell, k> &intermediate,
                              MSSshare<ell> *input_x, MSSshare_p *output_z);

/* 执行PI_wrap1_spec协议
 */
template <int ell, ShareValue k>
void PI_wrap1_spec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_wrap1_spec_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
                   MSSshare_p *output_z);

/* 执行PI_wrap2_spec协议
 */
template <int ell, ShareValue k>
void PI_wrap2_spec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_wrap2_spec_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
                   MSSshare_p *output_z);

/* 预处理PI_wrap1_intermediate对象和协议的输出
 */
template <int ell, ShareValue k>
void PI_wrap1_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_wrap1_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
                         MSSshare_p_add_res *output_z);

/* 预处理PI_wrap2_intermediate对象和协议的输出
 */
template <int ell, ShareValue k>
void PI_wrap2_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_wrap2_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
                         MSSshare_p_add_res *output_z);

/* 执行PI_wrap1协议
 */
template <int ell, ShareValue k>
void PI_wrap1(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_wrap1_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
              MSSshare_p_add_res *output_z);

/* 执行PI_wrap2协议
 */
template <int ell, ShareValue k>
void PI_wrap2(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_wrap2_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
              MSSshare_p_add_res *output_z);
#include "protocol/wrap.tpp"