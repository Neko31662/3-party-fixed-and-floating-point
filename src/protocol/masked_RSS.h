#pragma once

#include "config/config.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

template <int ell> class MSSshare {
  public:
    // For party 0, v1: r1, v2: r2
    // For party 1, v1: m , v2: r1
    // For party 2, v1: m , v2: r2
    ShareValue v1, v2;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    bool has_shared = false;
#endif
    static const ShareValue MASK;
    static const int BYTELEN;

    MSSshare() {
        if (ell > ShareValue_BitLength) {
            error("MSSshare only supports ell <= " + std::to_string(ShareValue_BitLength) + " now");
        }
    }

    virtual ~MSSshare() = default;

};

template <int ell> class MSSshare_add_res : public MSSshare<ell> {
  public:
#ifdef DEBUG_MODE
    using MSSshare<ell>::has_preprocess;
    using MSSshare<ell>::has_shared;
#endif
    using MSSshare<ell>::MASK;
    using MSSshare<ell>::BYTELEN;
    using MSSshare<ell>::v1;
    using MSSshare<ell>::v2;

    virtual ~MSSshare_add_res() = default;
};

template <int ell> class MSSshare_mul_res : public MSSshare<ell> {
  public:
#ifdef DEBUG_MODE
    using MSSshare<ell>::has_preprocess;
    using MSSshare<ell>::has_shared;
#endif
    using MSSshare<ell>::MASK;
    using MSSshare<ell>::BYTELEN;
    using MSSshare<ell>::v1;
    using MSSshare<ell>::v2;
    // To calculate s3 = s1 * s2:
    // For party 0, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 1, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 2, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_2
    ShareValue v3;

    virtual ~MSSshare_mul_res() = default;
};

template <int ell>
const ShareValue MSSshare<ell>::MASK =
    (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
template <int ell> const int MSSshare<ell>::BYTELEN = (ell + 7) / 8;

// ===============================================================
// ======================= implementation ========================
// ===============================================================

/* 重构MSSshare对象，返回重构后的秘密值
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的MSSshare对象指针
 */
template <int ell> ShareValue MSSshare_recon(const int party_id, NetIOMP &netio, MSSshare<ell> *s);

/* 预处理MSSshare对象
 * @param secret_holder_id: 秘密值持有方id，0/1,2，如果该对象之后不进行share_from，必须传入0
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param s: MSSshare对象指针
 */
template <int ell>
void MSSshare_preprocess(const int secret_holder_id, const int party_id, std::vector<PRGSync> &PRGs,
                         NetIOMP &netio, MSSshare<ell> *s);

/* 预处理MSSshare_add_res对象
 * @param party_id: 参与方id，0/1/2
 * @param res: 预处理的MSSshare_add_res对象指针
 * @param s1: 第一个作为加法输入的分享对象的指针
 * @param s2: 第二个作为加法输入的分享对象的指针
 */
template <int ell>
void MSSshare_add_res_preprocess(const int party_id, MSSshare_add_res<ell> *res,
                                 const MSSshare<ell> *s1, const MSSshare<ell> *s2);

/* 预处理MSSshare_mul_res对象
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param res: 预处理的MSSshare_mul_res对象指针
 * @param s1: 第一个作为乘法输入的分享对象的指针
 * @param s2: 第二个作为乘法输入的分享对象的指针
 */
template <int ell>
void MSSshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 MSSshare_mul_res<ell> *res, const MSSshare<ell> *s1,
                                 const MSSshare<ell> *s2);

/* 从secret_holder_id处获取秘密值x的MSSshare分享，存储在s中
 * @param secret_holder_id: 秘密值持有方id，0/1/2
 * @param party_id: 参与方id，0/1/2
 * @param x: 待分享的秘密值，仅secret_holder_id方提供有效值
 * @param netio: 多方通信接口
 * @param s: 存储分享结果的MSSshare对象指针
 */
template <int ell>
void MSSshare_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                         MSSshare<ell> *s, ShareValue x);

/*明文加法
 * @param party_id: 参与方id，0/1/2
 * @param s: 存储分享结果的MSSshare对象指针
 * @param x: 待加的明文值
 */
template <int ell> void MSSshare_add_plain(const int party_id, MSSshare<ell> *s, ShareValue x);

/*密文加法
 * @param party_id: 参与方id，0/1/2
 * @param res: 作为结果输出的MSSshare_add_res对象指针
 * @param s1: 第一个作为加法输入的分享对象的指针
 * @param s2: 第二个作为加法输入的分享对象的指针
 */
template <int ell>
void MSSshare_add_res_calc_add(const int party_id, MSSshare_add_res<ell> *res,
                               const MSSshare<ell> *s1, const MSSshare<ell> *s2);

/*密文乘法
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 作为结果输出的MSSshare_mul_res对象指针
 * @param s1: 第一个作为乘法输入的分享对象的指针
 * @param s2: 第二个作为乘法输入的分享对象的指针
 */
template <int ell>
void MSSshare_mul_res_calc_mul(const int party_id, NetIOMP &netio, MSSshare_mul_res<ell> *res,
                               const MSSshare<ell> *s1, const MSSshare<ell> *s2);

#include "protocol/masked_RSS.tpp"

// // 压缩函数：将vector<ShareValue>的前ell比特压缩到string
// template <int ell> std::string compress_shares(const std::vector<ShareValue> &values) {
//     if (values.empty())
//         return "";

//     const ShareValue MASK =
//         (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
//     const size_t num_values = values.size();
//     const size_t total_bits = num_values * ell;
//     const size_t total_bytes = (total_bits + 7) / 8; // 向上取整

//     std::string result(total_bytes, 0);

//     size_t bit_offset = 0;
//     for (size_t i = 0; i < num_values; i++) {
//         ShareValue masked_value = values[i] & MASK;

//         // 将ell比特的数据写入到result中
//         for (int bit = 0; bit < ell; bit++) {
//             if (masked_value & (ShareValue(1) << bit)) {
//                 size_t byte_pos = bit_offset / 8;
//                 size_t bit_pos = bit_offset % 8;
//                 result[byte_pos] |= (1 << bit_pos);
//             }
//             bit_offset++;
//         }
//     }

//     return result;
// }

// // 解压缩函数：从string解压到vector<ShareValue>
// template <int ell>
// std::vector<ShareValue> decompress_shares(const std::string &compressed, size_t num_values) {
//     if (compressed.empty() || num_values == 0)
//         return {};

//     const ShareValue MASK =
//         (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
//     std::vector<ShareValue> result(num_values, 0);

//     size_t bit_offset = 0;
//     for (size_t i = 0; i < num_values; i++) {
//         ShareValue value = 0;

//         // 从compressed中读取ell比特的数据
//         for (int bit = 0; bit < ell; bit++) {
//             size_t byte_pos = bit_offset / 8;
//             size_t bit_pos = bit_offset % 8;

//             if (byte_pos < compressed.size() && (compressed[byte_pos] & (1 << bit_pos))) {
//                 value |= (ShareValue(1) << bit);
//             }
//             bit_offset++;
//         }

//         result[i] = value & MASK;
//     }

//     return result;
// }

// template <int ell>
// void calc_mul_vec(std::vector<std::shared_ptr<MSSshare_mul_res<ell>>> shares, NetIOMP &netio) {
//     if (shares.empty())
//         return;

//     netio.sync();

//     const int num_shares = shares.size();
//     const ShareValue MASK = MSSshare<ell>::MASK;
//     const int party_id = MSSshare<ell>::party_id;

// #ifdef DEBUG_MODE
//     // 检查所有shares的状态
//     for (size_t i = 0; i < shares.size(); i++) {
//         if (!shares[i]->has_preprocess) {
//             error("calc_mul_vec: share[" + std::to_string(i) + "] must be preprocessed");
//         }
//         if (!shares[i]->s1->has_shared || !shares[i]->s2->has_shared) {
//             error("calc_mul_vec: share[" + std::to_string(i) + "]'s inputs must be shared");
//         }
//         shares[i]->has_shared = true;
//     }
// #endif

//     if (party_id == 1 || party_id == 2) {
//         // 准备发送数据的缓冲区
//         std::vector<ShareValue> send_data;
//         send_data.reserve(num_shares);

//         // 计算所有乘法的中间结果
//         for (int i = 0; i < num_shares; i++) {
//             auto &share = shares[i];
//             ShareValue temp = 0;

//             // 计算 s1.v1 * s2.v2 + s1.v2 * s2.v1 + v3 - v2
//             temp += share->s1->v1 * share->s2->v2;
//             temp += share->s1->v2 * share->s2->v1;
//             temp += share->v3;
//             temp -= share->v2;

//             // Party 1 还需要加上 s1.v1 * s2.v1
//             if (party_id == 1) {
//                 temp += share->s1->v1 * share->s2->v1;
//             }

//             temp &= MASK;
//             send_data.push_back(temp);
//         }

//         // 压缩发送数据
//         std::string compressed_send = compress_shares<ell>(send_data);
//         std::string compressed_recv;

//         // 计算压缩后的数据大小
//         const size_t compressed_size = (num_shares * ell + 7) / 8;
//         compressed_recv.resize(compressed_size);

//         // 一次性发送和接收压缩后的数据
//         if (party_id == 1) {
//             // Party 1 -> Party 2
//             netio.send_data(2, compressed_send.data(), compressed_send.size());
//             netio.recv_data(2, compressed_recv.data(), compressed_recv.size());
//         } else if (party_id == 2) {
//             // Party 2 <- Party 1
//             netio.recv_data(1, compressed_recv.data(), compressed_recv.size());
//             netio.send_data(1, compressed_send.data(), compressed_send.size());
//         }

//         // 解压缩接收到的数据
//         std::vector<ShareValue> recv_data = decompress_shares<ell>(compressed_recv, num_shares);

//         // 计算最终结果
//         for (int i = 0; i < num_shares; i++) {
//             shares[i]->v1 = (recv_data[i] + send_data[i]) & MASK;
//         }
//     }
// }