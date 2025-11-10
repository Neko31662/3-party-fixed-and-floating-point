#pragma once

#include "config/config.h"
#include "protocol/addictive_share_p.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

// 这里的加法分享是P1和P2之间的加法分享，P0起第三方辅助作用
template <int ell, typename ShareType = ShareValue> class ADDshare {
  public:
    ShareType v = 0;
#ifdef DEBUG_MODE
    bool has_shared = false;
#endif
    static const ShareType MASK;
    static const int BYTELEN;
    ADDshare() {
        if (ell > sizeof(ShareType) * 8) {
            error("ADDshare: ell exceeds ShareType bit length " +
                  std::to_string(sizeof(ShareType) * 8));
        }
    }
};

template <int ell, typename ShareType = ShareValue>
class ADDshare_mul_res : public ADDshare<ell, ShareType> {
  public:
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    using ADDshare<ell, ShareType>::has_shared;
#endif
    using ADDshare<ell, ShareType>::MASK;
    using ADDshare<ell, ShareType>::BYTELEN;
    using ADDshare<ell, ShareType>::v;

    ShareType x = 0, y = 0, z = 0;
};

template <int ell, typename ShareType>
const ShareType ADDshare<ell, ShareType>::MASK =
    (ell == ShareValue_BitLength) ? ~ShareType(0) : ((ShareType(1) << ell) - 1);
template <int ell, typename ShareType> const int ADDshare<ell, ShareType>::BYTELEN = (ell + 7) / 8;

// ===============================================================
// ======================= implementation ========================
// ===============================================================

/* 从secret_holder_id处获取秘密值x的ADDshare分享，存储在s中
 * @param secret_holder_id: 秘密值持有方id，0/1/2
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param s: 存储分享结果的ADDshare对象指针
 * @param x: 待分享的秘密值，仅secret_holder_id方提供有效值
 */
template <int ell, typename ShareType>
inline void ADDshare_share_from(const int secret_holder_id, const int party_id,
                                std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                ADDshare<ell, ShareType> *s, ShareType x);

/*与ADDshare_share_from类似，但将分享结果存储在netio的缓存中，等待后续发送，此时secret_holder_id必须为0
 */
template <int ell, typename ShareType>
inline void ADDshare_share_from_store(const int party_id, std::vector<PRGSync> &PRGs,
                                      NetIOMP &netio, ADDshare<ell, ShareType> *s, ShareType x);

/* 预处理ADDshare_mul_res对象，由P0为P1和P2生成Beaver三元组
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param res: 预处理的ADDshare_mul_res对象指针
 */
template <int ell, typename ShareType>
inline void ADDshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs,
                                        NetIOMP &netio, ADDshare_mul_res<ell, ShareType> *res);

/* 计算两个ADDshare的乘法结果，结果存储在res中，仅P1和P2参与计算
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 乘法结果存储的ADDshare_mul_res对象指针
 * @param s1: 第一个加法分享对象指针
 * @param s2: 第二个加法分享对象指针
 */
template <int ell, typename ShareType>
inline void ADDshare_mul_res_cal_mult(const int party_id, NetIOMP &netio,
                                      ADDshare_mul_res<ell, ShareType> *res,
                                      ADDshare<ell, ShareType> *s1, ADDshare<ell, ShareType> *s2);

/* 重构ADDshare对象，返回重构后的秘密值，仅P1,P2参与重构
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的ADDshare对象指针
 */
template <int ell, typename ShareType>
inline ShareType ADDshare_recon(const int party_id, NetIOMP &netio, ADDshare<ell, ShareType> *s);

/* ADDshare_mul_res_cal_mult的向量化版本
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 乘法结果存储的ADDshare_mul_res对象指针
 * @param s1: 第一个加法分享对象指针
 * @param s2: 第二个加法分享对象指针
 */
template <int ell, typename ShareType>
inline void
ADDshare_mul_res_cal_mult_vec(const int party_id, NetIOMP &netio,
                              const std::vector<ADDshare_mul_res<ell, ShareType> *> &res,
                              const std::vector<ADDshare<ell, ShareType> *> &s1,
                              const std::vector<ADDshare<ell, ShareType> *> &s2);

template <int ell> inline void ADDshare_from_p(ADDshare<ell> *to, ADDshare_p *from);

template <int ell>
inline void ADDshare_mul_res_from_p(ADDshare_mul_res<ell> *to, ADDshare_p_mul_res *from);
#include "protocol/addictive_share.tpp"