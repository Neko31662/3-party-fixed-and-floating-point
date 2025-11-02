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
    static int party_id;
    static const ShareValue MASK;
    static const int BYTELEN;

    MSSshare() {
        if (ell > 64) {
            error("MSSshare only supports ell <= 64 now");
        }
        if (party_id == -1) {
            error("MSSshare party_id not set");
        }
    }

    // P0.PRGs[0] <----sync----> P1.PRGs[0]
    // P0.PRGs[1] <----sync----> P2.PRGs[0]
    // If party i holds the secret to be shared: secret_holder_id = i
    // If this share is an output of two shares' calculation: secret_holder_id = 0
    void preprocess(int secret_holder_id, std::vector<std::shared_ptr<PRGSync>> &PRGs);

    // invoke this after preprocess
    void share_from(int secret_holder_id, ShareValue x, NetIOMP &netio);

    ShareValue recon(NetIOMP &netio);
};

template <int ell> class MSSshare_add_res : public MSSshare<ell> {
  public:
    using MSSshare<ell>::has_preprocess;
    using MSSshare<ell>::has_shared;
    using MSSshare<ell>::party_id;
    using MSSshare<ell>::MASK;
    using MSSshare<ell>::BYTELEN;
    using MSSshare<ell>::v1;
    using MSSshare<ell>::v2;

    std::shared_ptr<MSSshare<ell>> s1, s2;

    void share_from(int secret_holder_id, ShareValue x, NetIOMP &netio) = delete;

    void preprocess(const std::shared_ptr<MSSshare<ell>> &s1,
                    const std::shared_ptr<MSSshare<ell>> &s2);

    void calc_add();
};

template <int ell> class MSSshare_mul_res : public MSSshare<ell> {
  public:
    using MSSshare<ell>::has_preprocess;
    using MSSshare<ell>::has_shared;
    using MSSshare<ell>::party_id;
    using MSSshare<ell>::MASK;
    using MSSshare<ell>::BYTELEN;
    using MSSshare<ell>::v1;
    using MSSshare<ell>::v2;
    // To calculate s3 = s1 * s2:
    // For party 0, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 1, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 2, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_2
    ShareValue v3;
    std::shared_ptr<MSSshare<ell>> s1, s2;

    void share_from(int secret_holder_id, ShareValue x, NetIOMP &netio) = delete;

    // P0.PRGs[0] <----sync----> P1.PRGs[0]
    // P0.PRGs[1] <----sync----> P2.PRGs[0]
    void preprocess(std::vector<std::shared_ptr<PRGSync>> &PRGs, NetIOMP &netio,
                    const std::shared_ptr<MSSshare<ell>> &s1,
                    const std::shared_ptr<MSSshare<ell>> &s2);

    void calc_mul(NetIOMP &netio);
};

template <int ell> int MSSshare<ell>::party_id = -1;
template <int ell>
const ShareValue MSSshare<ell>::MASK = (ell == 64) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
template <int ell> const int MSSshare<ell>::BYTELEN = (ell + 7) / 8;

template <int ell> inline void set_MSSshare_party_id(int id) {
    if (MSSshare<ell>::party_id != -1) {
        error("MSSshare::party_id can only be set once");
    }
    MSSshare<ell>::party_id = id;
};

template <int ell> ShareValue MSSshare<ell>::recon(NetIOMP &netio) {
    netio.sync();
    ShareValue total;
    if (party_id == 0) {
        netio.send_data(1, &v2, BYTELEN);
        netio.send_data(2, &v1, BYTELEN);

        netio.recv_data(1, &total, BYTELEN);
    } else if (party_id == 1) {
        netio.recv_data(0, &total, BYTELEN);
        netio.send_data(0, &v1, BYTELEN);

    } else if (party_id == 2) {
        netio.recv_data(0, &total, BYTELEN);
    }
    total = (total + v1 + v2) & MASK;
    return total;
}

template <int ell>
void MSSshare<ell>::preprocess(int secret_holder_id, std::vector<std::shared_ptr<PRGSync>> &PRGs) {

#ifdef DEBUG_MODE
    has_preprocess = true;
#endif

    if (secret_holder_id != 2) {
        if (party_id == 0) {
            PRGs[0]->gen_random_data(&v1, BYTELEN);
            v1 &= MASK;
        } else if (party_id == 1) {
            PRGs[0]->gen_random_data(&v2, BYTELEN);
            v2 &= MASK;
        }
    }

    if (secret_holder_id != 1) {
        if (party_id == 0) {
            PRGs[1]->gen_random_data(&v2, BYTELEN);
            v2 &= MASK;
        } else if (party_id == 2) {
            PRGs[0]->gen_random_data(&v2, BYTELEN);
            v2 &= MASK;
        }
    }
}

template <int ell>
void MSSshare<ell>::share_from(int secret_holder_id, ShareValue x, NetIOMP &netio) {

#ifdef DEBUG_MODE
    if (!has_preprocess) {
        error("MSSshare::share must be invoked after MSSshare::preprocess");
    }
    has_shared = true;
#endif

    // Party 0 holds secret x
    if (secret_holder_id == 0) {
        if (party_id == 0) {
            ShareValue m = x;
            m -= v1;
            m -= v2;
            m &= MASK;
            // send m = x - r1 - r2
            netio.store_data(1, &m, BYTELEN);
            netio.store_data(2, &m, BYTELEN);
            // need to send later
        } else {
            netio.recv_data(0, &v1, BYTELEN);
            v1 &= MASK;
        }
    }
    // Party 1,2 holds secret x
    else if (secret_holder_id == 1 || secret_holder_id == 2) {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id) {
            v1 = x;
            v1 -= v2;
            v1 &= MASK;
            // send m = x - r1(or r2)
            netio.store_data(other_party, &v1, BYTELEN);
            // need to send later
        } else if (party_id == other_party) {
            netio.recv_data(secret_holder_id, &v1, BYTELEN);
            v1 &= MASK;
        }
    } else {
        error("MSSshare::share_from only supports secret_holder_id in {0,1,2}");
    }
}

template <int ell>
void MSSshare_add_res<ell>::preprocess(const std::shared_ptr<MSSshare<ell>> &s1,
                                       const std::shared_ptr<MSSshare<ell>> &s2) {

#ifdef DEBUG_MODE
    if (!s1->has_preprocess || !s2->has_preprocess) {
        error("MSSshare_add_res::preprocess requires s1 and s2 to have been preprocessed");
    }
    this->has_preprocess = true;
#endif

    this->s1 = s1;
    this->s2 = s2;

    this->v1 = (s1->v1 + s2->v1) & this->MASK;
    this->v2 = (s1->v2 + s2->v2) & this->MASK;
}

template <int ell> void MSSshare_add_res<ell>::calc_add() {

#ifdef DEBUG_MODE
    if (!s1->has_shared || !s2->has_shared) {
        error("MSSshare_add_res::calc_add requires both inputs to have been shared");
    }
    has_shared = true;
#endif

    v1 = (s1->v1 + s2->v1) & MASK;
    v2 = (s1->v2 + s2->v2) & MASK;
}

template <int ell>
void MSSshare_mul_res<ell>::preprocess(std::vector<std::shared_ptr<PRGSync>> &PRGs, NetIOMP &netio,
                                       const std::shared_ptr<MSSshare<ell>> &s1,
                                       const std::shared_ptr<MSSshare<ell>> &s2) {
    MSSshare<ell>::preprocess(0, PRGs);

#ifdef DEBUG_MODE
    if (!s1->has_preprocess || !s2->has_preprocess) {
        error("MSSshare_mul_res::preprocess requires s1 and s2 to have been preprocessed");
    }
    this->has_preprocess = true;
#endif

    this->s1 = s1;
    this->s2 = s2;
    if (party_id == 0 || party_id == 1) {
        PRGs[0]->gen_random_data(&v3, BYTELEN);
        v3 &= MASK;
    }
    if (party_id == 0) {
        ShareValue temp = (s1->v1 + s1->v2) * (s2->v1 + s2->v2);
        temp -= v3;
        temp &= MASK;
        netio.store_data(2, &temp, BYTELEN);
        // need to send later
    } else if (party_id == 2) {
        netio.recv_data(0, &v3, BYTELEN);
        v3 &= MASK;
    }
}

template <int ell> void MSSshare_mul_res<ell>::calc_mul(NetIOMP &netio) {
    netio.sync();
#ifdef DEBUG_MODE
    if (!has_preprocess) {
        error("MSSshare_mul_res::calc_mul must be invoked after "
              "MSSshare_mul_res::preprocess");
    }
    if (!s1->has_shared || !s2->has_shared) {
        error("MSSshare_mul_res::calc_mul requires s1 and s2 to have been shared");
    }
    has_shared = true;
#endif
    if (party_id == 1 || party_id == 2) {
        ShareValue temp = 0;
        ShareValue recv_temp;
        temp += s1->v1 * s2->v2;
        temp += s1->v2 * s2->v1;
        temp += v3;
        temp -= v2;

        if (party_id == 1) {
            temp += s1->v1 * s2->v1;
            netio.send_data(2, &temp, BYTELEN);

            netio.recv_data(2, &recv_temp, BYTELEN);
        } else if (party_id == 2) {
            netio.recv_data(1, &recv_temp, BYTELEN);
            netio.send_data(1, &temp, BYTELEN);
        }
        v1 = (recv_temp + temp) & MASK;
    }
}

// 压缩函数：将vector<ShareValue>的前ell比特压缩到string
template <int ell>
std::string compress_shares(const std::vector<ShareValue>& values) {
    if (values.empty()) return "";
    
    const ShareValue MASK = (ell == 64) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
    const size_t num_values = values.size();
    const size_t total_bits = num_values * ell;
    const size_t total_bytes = (total_bits + 7) / 8;  // 向上取整
    
    std::string result(total_bytes, 0);
    
    size_t bit_offset = 0;
    for (size_t i = 0; i < num_values; i++) {
        ShareValue masked_value = values[i] & MASK;
        
        // 将ell比特的数据写入到result中
        for (int bit = 0; bit < ell; bit++) {
            if (masked_value & (ShareValue(1) << bit)) {
                size_t byte_pos = bit_offset / 8;
                size_t bit_pos = bit_offset % 8;
                result[byte_pos] |= (1 << bit_pos);
            }
            bit_offset++;
        }
    }
    
    return result;
}

// 解压缩函数：从string解压到vector<ShareValue>
template <int ell>
std::vector<ShareValue> decompress_shares(const std::string& compressed, size_t num_values) {
    if (compressed.empty() || num_values == 0) return {};
    
    const ShareValue MASK = (ell == 64) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
    std::vector<ShareValue> result(num_values, 0);
    
    size_t bit_offset = 0;
    for (size_t i = 0; i < num_values; i++) {
        ShareValue value = 0;
        
        // 从compressed中读取ell比特的数据
        for (int bit = 0; bit < ell; bit++) {
            size_t byte_pos = bit_offset / 8;
            size_t bit_pos = bit_offset % 8;
            
            if (byte_pos < compressed.size() && 
                (compressed[byte_pos] & (1 << bit_pos))) {
                value |= (ShareValue(1) << bit);
            }
            bit_offset++;
        }
        
        result[i] = value & MASK;
    }
    
    return result;
}

template <int ell>
void calc_mul_vec(std::vector<std::shared_ptr<MSSshare_mul_res<ell>>> shares, NetIOMP &netio) {
    if (shares.empty()) return;
    
    netio.sync();
    
    const int num_shares = shares.size();
    const ShareValue MASK = MSSshare<ell>::MASK;
    const int party_id = MSSshare<ell>::party_id;
    
#ifdef DEBUG_MODE
    // 检查所有shares的状态
    for (size_t i = 0; i < shares.size(); i++) {
        if (!shares[i]->has_preprocess) {
            error("calc_mul_vec: share[" + std::to_string(i) + "] must be preprocessed");
        }
        if (!shares[i]->s1->has_shared || !shares[i]->s2->has_shared) {
            error("calc_mul_vec: share[" + std::to_string(i) + "]'s inputs must be shared");
        }
        shares[i]->has_shared = true;
    }
#endif
    
    if (party_id == 1 || party_id == 2) {
        // 准备发送数据的缓冲区
        std::vector<ShareValue> send_data;
        send_data.reserve(num_shares);
        
        // 计算所有乘法的中间结果
        for (int i = 0; i < num_shares; i++) {
            auto& share = shares[i];
            ShareValue temp = 0;
            
            // 计算 s1.v1 * s2.v2 + s1.v2 * s2.v1 + v3 - v2
            temp += share->s1->v1 * share->s2->v2;
            temp += share->s1->v2 * share->s2->v1;
            temp += share->v3;
            temp -= share->v2;
            
            // Party 1 还需要加上 s1.v1 * s2.v1
            if (party_id == 1) {
                temp += share->s1->v1 * share->s2->v1;
            }
            
            temp &= MASK;
            send_data.push_back(temp);
        }
        
        // 压缩发送数据
        std::string compressed_send = compress_shares<ell>(send_data);
        std::string compressed_recv;
        
        // 计算压缩后的数据大小
        const size_t compressed_size = (num_shares * ell + 7) / 8;
        compressed_recv.resize(compressed_size);
        
        // 一次性发送和接收压缩后的数据
        if (party_id == 1) {
            // Party 1 -> Party 2
            netio.send_data(2, compressed_send.data(), compressed_send.size());
            netio.recv_data(2, compressed_recv.data(), compressed_recv.size());
        } else if (party_id == 2) {
            // Party 2 <- Party 1
            netio.recv_data(1, compressed_recv.data(), compressed_recv.size());
            netio.send_data(1, compressed_send.data(), compressed_send.size());
        }
        
        // 解压缩接收到的数据
        std::vector<ShareValue> recv_data = decompress_shares<ell>(compressed_recv, num_shares);
        
        // 计算最终结果
        for (int i = 0; i < num_shares; i++) {
            shares[i]->v1 = (recv_data[i] + send_data[i]) & MASK;
        }
    }
}