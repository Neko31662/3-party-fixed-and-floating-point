#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "utils/misc.h"
#include "utils/permutation.h"

template <int ell, ShareValue k> struct PI_sign_intermediate {
    constexpr static ShareValue p = nxt_prime(ell);
    MSSshare_p r_prime{k};
    ShareValue Gamma = 0;
    std::array<ShareValue, ell> rx_list{};
    bool Delta;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
};

/* 预处理PI_sign_preprocess对象和协议的输出，调用后P0和P1都需要向P2发送暂存的信息
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param intermediate: 预处理的PI_sign_preprocess对象引用
 * @param input_x: 作为协议输入的MSSshare对象指针，需要已经预处理完成
 * @param output_b: 协议输出的对象的指针
 *
 * @templateparam ell: 输入分享的位数，输入分享必须在2^ell上
 * @templateparam k: 输出分享的模数
 */
template <int ell, ShareValue k>
void PI_sign_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                        PI_sign_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
                        MSSshare_p *output_b);

/* 执行PI_sign协议
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param intermediate: 预处理的PI_sign_preprocess对象引用
 * @param input_x: 作为协议输入的MSSshare对象指针，需要已经分享完成
 * @param output_b: 协议输出的对象的指针
 *
 * @templateparam ell: 输入分享的位数，输入分享必须在2^ell上
 * @templateparam k: 输出分享的模数
 */
template <int ell, ShareValue k>
void PI_sign(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
             PI_sign_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
             MSSshare_p *output_b);

template <int ell, ShareValue k>
void PI_sign_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 std::vector<PI_sign_intermediate<ell, k> *> &intermediate_vec,
                 std::vector<MSSshare<ell> *> input_x_vec, std::vector<MSSshare_p *> output_b_vec);

#include "protocol/sign.tpp"