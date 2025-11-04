#pragma once

#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

template <int l, int lf> class fixed_point_share {
  public:
    static const ShareValue MASK_l;
    static const ShareValue MASK_lf;
    static const ShareValue MASK_llf;
    std::shared_ptr<MSSshare<l + lf>> int_share; // share x in l+lf bits

    fixed_point_share() {
#ifdef DEBUG_MODE
        if (l <= lf) {
            error("fixed_point_share: l must be greater than lf");
        }
        if (lf <= 0) {
            error("fixed_point_share: lf must be positive");
        }
#endif
        int_share = std::make_shared<MSSshare<l + lf>>();
    }

    void preprocess(int secret_holder_id, std::vector<std::shared_ptr<PRGSync>> &PRGs);

    void share_from(int secret_holder_id, ShareValue x, NetIOMP &netio);

    ShareValue recon(NetIOMP &netio);

    // w0 = the wrap bit of r1 + r2
    int get_w0();
};

template <int l, int lf> class fixed_point_share_add_res : public fixed_point_share<l, lf> {
  public:
    using fixed_point_share<l, lf>::int_share;
    using fixed_point_share<l, lf>::MASK_l;
    using fixed_point_share<l, lf>::MASK_lf;
    using fixed_point_share<l, lf>::MASK_llf;

    std::shared_ptr<fixed_point_share<l, lf>> s1, s2;

    fixed_point_share_add_res() { int_share = std::make_shared<MSSshare_add_res<l + lf>>(); }

    // x是带符号值
    void share_from(int secret_holder_id, ShareValue x, NetIOMP &netio) = delete;

    void preprocess(const std::shared_ptr<fixed_point_share<l, lf>> &s1,
                    const std::shared_ptr<fixed_point_share<l, lf>> &s2);

    void calc_add();
};

template <int l, int lf> class fixed_point_share_mul_res : public fixed_point_share<l, lf> {
  public:
    using fixed_point_share<l, lf>::int_share;
    using fixed_point_share<l, lf>::MASK_l;
    using fixed_point_share<l, lf>::MASK_lf;
    using fixed_point_share<l, lf>::MASK_llf;

    struct preprocess_msg {
        ShareValue v1_1; // (s1.r1+s1.r2)*(s2.r1+s2.r2)
    };

    preprocess_msg msg1, msg2;

    std::shared_ptr<fixed_point_share<l, lf>> s1, s2;

    void preprocess(std::vector<std::shared_ptr<PRGSync>> &PRGs, NetIOMP &netio,
                    const std::shared_ptr<fixed_point_share<l, lf>> &s1,
                    const std::shared_ptr<fixed_point_share<l, lf>> &s2);
};

template <int l, int lf>
const ShareValue fixed_point_share<l, lf>::MASK_l =
    (l == 64) ? ~ShareValue(0) : ((ShareValue(1) << l) - 1);
template <int l, int lf>
const ShareValue fixed_point_share<l, lf>::MASK_lf =
    (lf == 64) ? ~ShareValue(0) : ((ShareValue(1) << lf) - 1);
template <int l, int lf>
const ShareValue fixed_point_share<l, lf>::MASK_llf =
    (l + lf == 64) ? ~ShareValue(0) : ((ShareValue(1) << (l + lf)) - 1);

// ===============================================================
// ======================= implementation ========================
// ===============================================================

inline bool get_bit_of_pos(ShareValue x, int pos) { return (x >> pos) & 1; }

template <int l, int lf> void set_fixed_point_share_party_id(int id) {
    set_MSSshare_party_id<l + lf>(id);
};

template <int l, int lf>
void fixed_point_share<l, lf>::preprocess(int secret_holder_id,
                                          std::vector<std::shared_ptr<PRGSync>> &PRGs) {
    int_share->preprocess(secret_holder_id, PRGs);
}

template <int l, int lf>
void fixed_point_share<l, lf>::share_from(int secret_holder_id, ShareValue x, NetIOMP &netio) {

#ifdef DEBUG_MODE
    // x should be in [0, 2^(l-2)) or [-2^(l-2), 0)
    ShareValue tmp1(1), tmp2(1);
    tmp1 = (tmp1 << (l - 2));        // 2^(l-2)
    tmp2 = (tmp2 << (l - 1));        // 2^(l-1)
    ShareValue x_prime = (x + tmp1); // x' = x + 2^(l-2)
    if (x_prime >= tmp2) {
        error("fixed_point_share::share_from: x is out of range");
    }
#endif

    int_share->share_from(secret_holder_id, x, netio);
}

template <int l, int lf> ShareValue fixed_point_share<l, lf>::recon(NetIOMP &netio) {
    return int_share->recon(netio);
}

template <int l, int lf> int fixed_point_share<l, lf>::get_w0() {

#ifdef DEBUG_MODE
    if (!int_share->has_preprocess) {
        error("fixed_point_share::get_w0: must call preprocess before get_w0");
    }
    if (MSSshare<l + lf>::party_id != 0) {
        error("fixed_point_share::get_w0: only party 0 can get w0");
    }
#endif

    ShareValue tmp1 = int_share->v1 & MASK_l;
    ShareValue tmp2 = int_share->v2 & MASK_l;
    return (tmp1 + tmp2) >> l;
}

template <int l, int lf>
void fixed_point_share_add_res<l, lf>::preprocess(
    const std::shared_ptr<fixed_point_share<l, lf>> &s1,
    const std::shared_ptr<fixed_point_share<l, lf>> &s2) {
    this->s1 = s1;
    this->s2 = s2;
    std::static_pointer_cast<MSSshare_add_res<l + lf>>(int_share)->preprocess(s1->int_share,
                                                                              s2->int_share);
}

template <int l, int lf> void fixed_point_share_add_res<l, lf>::calc_add() {
    std::static_pointer_cast<MSSshare_add_res<l + lf>>(int_share)->calc_add();
}

// template <int l, int lf>
// void fixed_point_share_mul_res<l, lf>::preprocess(
//     std::vector<std::shared_ptr<PRGSync>> &PRGs, NetIOMP &netio,
//     const std::shared_ptr<fixed_point_share<l, lf>> &s1,
//     const std::shared_ptr<fixed_point_share<l, lf>> &s2) {

//     this->s1 = s1;
//     this->s2 = s2;

//     auto int_s1 = s1->int_share;
//     auto int_s2 = s2->int_share;
//     auto int_s3 = this->int_share;

//     // For x*y
//     // P0 shares (s1.r1+s1.r2)*(s2.r1+s2.r2) to P1 and P2
//     int_share->MSSshare<l + lf>::preprocess(0, PRGs);

// #ifdef DEBUG_MODE
//     if (!int_s1->has_preprocess || !int_s2->has_preprocess) {
//         error("fixed_point_share_mul_res::preprocess requires s1 and s2 to have been preprocessed");
//     }
//     int_s3->has_preprocess = true;
// #endif

//     if (party_id == 0 || party_id == 1) {
//         PRGs[0]->gen_random_data(&int_s3->v3, MSSshare<l + lf>::BYTELEN);
//         int_s3->v3 &= MSSshare<l + lf>::MASK;
//     }
//     if (party_id == 0) {
//         ShareValue temp = (int_s1->v1 + int_s1->v2) * (int_s2->v1 + int_s2->v2);
//         temp -= int_s3->v3;
//         temp &= MSSshare<l + lf>::MASK;
//         msg0.add_msg((char *)&temp, l + lf);
//     } else if (party_id == 2) {
//         msg0.read_msg((char *)&int_s3->v3, l + lf);
//         int_s3->v3 &= MSSshare<l + lf>::MASK;
//     }

//     // For 2^(l+lf) * w_y * x
//     if (party_id == 0) {
//     }
// }