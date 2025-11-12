#pragma once

#include "config/config.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

class MSSshare_p {
  public:
    // For party 0, v1: r1, v2: r2
    // For party 1, v1: m , v2: r1
    // For party 2, v1: m , v2: r2
    ShareValue v1 = 0, v2 = 0;
    // mod p
    ShareValue p;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    bool has_shared = false;
#endif
    int BITLEN;
    int BYTELEN;

    MSSshare_p(ShareValue p) {
        this->p = p;
        BITLEN = 0;
        p--;
        while (p) {
            p >>= 1;
            BITLEN++;
        }
        BYTELEN = (BITLEN + 7) / 8;
        if (BITLEN > sizeof(ShareValue) * 8 / 2 && this->p != (ShareValue(1) << BITLEN)) {
            error("MSSshare_p: length of p exceeds ShareValue/2 =" +
                  std::to_string(sizeof(ShareValue) * 8 / 2) + ", and p is not power of 2");
        }
    }

    virtual ~MSSshare_p() = default;
    MSSshare_p &operator=(const ShareValue &) = delete;
};

class MSSshare_p_mul_res : public MSSshare_p {
  public:
#ifdef DEBUG_MODE
    using MSSshare_p::has_preprocess;
    using MSSshare_p::has_shared;
#endif
    using MSSshare_p::BITLEN;
    using MSSshare_p::BYTELEN;
    using MSSshare_p::p;
    using MSSshare_p::v1;
    using MSSshare_p::v2;
    // To calculate s3 = s1 * s2:
    // For party 0, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 1, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 2, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_2
    ShareValue v3 = 0;

    MSSshare_p_mul_res(ShareValue p) : MSSshare_p(p) {}
    virtual ~MSSshare_p_mul_res() = default;
    MSSshare_p_mul_res &operator=(const ShareValue &) = delete;
};

// ===============================================================
// ======================= implementation ========================
// ===============================================================

/* 重构MSSshare_p对象，返回重构后的秘密值
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的MSSshare_p对象指针
 */
inline ShareValue MSSshare_p_recon(const int party_id, NetIOMP &netio, MSSshare_p *s);

/* 预处理MSSshare_p对象
 * @param secret_holder_id: 秘密值持有方id，0/1,2，如果该对象之后不进行share_from，必须传入0
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param s: MSSshare_p对象指针
 */
inline void MSSshare_p_preprocess(const int secret_holder_id, const int party_id,
                                  std::vector<PRGSync> &PRGs, NetIOMP &netio, MSSshare_p *s);

/* 预处理MSSshare_p_add_res对象
 * @param party_id: 参与方id，0/1/2
 * @param res: 预处理的MSSshare_p_add_res对象指针
 * @param s1: 第一个作为加法输入的分享对象的指针
 * @param s2: 第二个作为加法输入的分享对象的指针
 */
inline void MSSshare_p_add_res_preprocess(const int party_id, MSSshare_p *res, MSSshare_p *s1,
                                          MSSshare_p *s2);

/* 预处理MSSshare_p_add_res对象，结果为多个分享对象的带系数累加
 * @param party_id: 参与方id，0/1/2
 * @param res: 预处理的MSSshare_p_add_res对象指针
 * @param s_vec: 多个作为加法输入的分享对象的指针数组
 * @param coeff_vec: 对应的明文系数数组，可以是负数
 */
inline void MSSshare_p_add_res_preprocess_multi(const int party_id, MSSshare_p *res,
                                                std::vector<MSSshare_p *> &s_vec,
                                                std::vector<int> &coeff_vec);

/* 预处理MSSshare_p_mul_res对象
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param res: 预处理的MSSshare_p_mul_res对象指针
 * @param s1: 第一个作为乘法输入的分享对象的指针
 * @param s2: 第二个作为乘法输入的分享对象的指针
 */
inline void MSSshare_p_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs,
                                          NetIOMP &netio, MSSshare_p_mul_res *res,
                                          const MSSshare_p *s1, const MSSshare_p *s2);

/* 从secret_holder_id处获取秘密值x的MSSshare_p分享，存储在s中
 * @param secret_holder_id: 秘密值持有方id，0/1/2
 * @param party_id: 参与方id，0/1/2
 * @param x: 待分享的秘密值，仅secret_holder_id方提供有效值
 * @param netio: 多方通信接口
 * @param s: 存储分享结果的MSSshare_p对象指针
 */
inline void MSSshare_p_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                                  MSSshare_p *s, ShareValue x);

/*与MSSshare_p_share_from类似，但将分享结果存储在netio的缓存中，等待后续发送，此时secret_holder_id必须为0
 */
inline void MSSshare_p_share_from_store(const int party_id, NetIOMP &netio, MSSshare_p *s,
                                        ShareValue x);

/*明文加法
 * @param party_id: 参与方id，0/1/2
 * @param s: 存储分享结果的MSSshare_p对象指针
 * @param x: 待加的明文值
 */
inline void MSSshare_p_add_plain(const int party_id, MSSshare_p *s, ShareValue x);

/*密文加法
 * @param party_id: 参与方id，0/1/2
 * @param res: 作为结果输出的MSSshare_p_add_res对象指针
 * @param s1: 第一个作为加法输入的分享对象的指针
 * @param s2: 第二个作为加法输入的分享对象的指针
 */
inline void MSSshare_p_add_res_calc_add(const int party_id, MSSshare_p *res, MSSshare_p *s1,
                                        MSSshare_p *s2);

/*密文加法，带系数的多重累加
 * @param party_id: 参与方id，0/1/2
 * @param res: 作为结果输出的MSSshare_p_add_res对象指针
 * @param s_vec: 多个作为加法输入的分享对象的指针数组
 * @param coeff_vec: 对应的明文系数数组，可以是负数
 */
inline void MSSshare_p_add_res_calc_add_multi(const int party_id, MSSshare_p *res,
                                              std::vector<MSSshare_p *> &s_vec,
                                              std::vector<int> &coeff_vec);

/*密文乘法
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 作为结果输出的MSSshare_p_mul_res对象指针
 * @param s1: 第一个作为乘法输入的分享对象的指针
 * @param s2: 第二个作为乘法输入的分享对象的指针
 */
inline void MSSshare_p_mul_res_calc_mul(const int party_id, NetIOMP &netio, MSSshare_p_mul_res *res,
                                        const MSSshare_p *s1, const MSSshare_p *s2);

/*密文乘法的向量化版本
 */
inline void MSSshare_p_mul_res_calc_mul_vec(const int party_id, NetIOMP &netio,
                                            std::vector<MSSshare_p_mul_res *> &res_vec,
                                            std::vector<MSSshare_p *> &s1_vec,
                                            std::vector<MSSshare_p *> &s2_vec);

#include "protocol/masked_RSS_p.tpp"