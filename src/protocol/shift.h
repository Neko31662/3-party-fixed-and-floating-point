#pragma once
#include "config/config.h"
#include "protocol/addictive_share.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"

// 将[0,ell-1]内的所有整数表示为二进制，并保证最高位必须为0，满足这一条件的最短二进制数长度即为LOG_1(ell)
#define LOG_1(ell) (highest_bit_pos(ell - 1) + 1)
// 协议所用的特殊加法分享的长度
#define EXPANDED_ELL(ell) (ell + (1 << LOG_1(ell)))

struct PI_shift_intermediate {
    int ell;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
    ADDshare_mul_res<> gamma1, gamma2, gamma3;
    ADDshare_mul_res<LongShareValue> d_prime;
    MSSshare two_pow_k;

    PI_shift_intermediate(int ell)
        : gamma1(ell), gamma2(ell), gamma3(ell), d_prime(EXPANDED_ELL(ell)), two_pow_k(ell) {
        this->ell = ell;
    }
};

/* 预处理PI_shift_intermediate对象和协议的输出
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param intermediate: 预处理的PI_shift_intermediate对象引用
 * @param input_x: 作为协议输入的MSSshare对象指针，需要已经预处理完成
 * @param output_res: 协议输出的对象的指针
 */
void PI_shift_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_shift_intermediate &intermediate, MSSshare *input_x,
                         MSSshare_mul_res *output_res);

void PI_shift(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_shift_intermediate &intermediate, MSSshare *input_x, ADDshare<> *input_k,
              MSSshare_mul_res *output_res);

#include "protocol/shift.tpp"