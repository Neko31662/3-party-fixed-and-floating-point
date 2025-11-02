#pragma once

#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

template <int l, int lf> class fixed_point_share {
  public:
    std::shared_ptr<MSSshare<l + lf>> int_share; // share x in l+lf bits

    fixed_point_share() {
#ifdef DEBUG_MODE
        if (l <= lf) {
            error("fixed_point_share: l must be greater than lf");
        }
#endif
        int_share = std::make_shared<MSSshare<l + lf>>();
    }

    void preprocess(int secret_holder_id, std::vector<std::shared_ptr<PRGSync>> &PRGs);

    void share_from(int secret_holder_id, ShareValue x, NetIOMP &netio);

    ShareValue recon(NetIOMP &netio);
};

template <int l, int lf> class fixed_point_share_add_res : public fixed_point_share<l, lf> {
  public:
    using fixed_point_share<l, lf>::int_share;

    std::shared_ptr<fixed_point_share<l, lf>> s1, s2;

    fixed_point_share_add_res() { int_share = std::make_shared<MSSshare_add_res<l + lf>>(); }

    void share_from(int secret_holder_id, ShareValue x, NetIOMP &netio) = delete;

    void preprocess(const std::shared_ptr<fixed_point_share<l, lf>> &s1,
                    const std::shared_ptr<fixed_point_share<l, lf>> &s2);

    void calc_add();
};

template <int l, int lf> class fixed_point_share_mul_res : public fixed_point_share<l, lf> {
  public:
    using fixed_point_share<l, lf>::int_share;

    std::shared_ptr<fixed_point_share<l, lf>> s1, s2;

    void preprocess(std::vector<std::shared_ptr<PRGSync>> &PRGs, NetIOMP &netio,
                    const std::shared_ptr<fixed_point_share<l, lf>> &s1,
                    const std::shared_ptr<fixed_point_share<l, lf>> &s2);
};

// ===============================================================
// ======================= implementation ========================
// ===============================================================

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

template <int l, int lf>
void fixed_point_share_mul_res<l, lf>::preprocess(
    std::vector<std::shared_ptr<PRGSync>> &PRGs, NetIOMP &netio,
    const std::shared_ptr<fixed_point_share<l, lf>> &s1,
    const std::shared_ptr<fixed_point_share<l, lf>> &s2) {
    int_share->preprocess(PRGs, netio, s1->int_share, s2->int_share);
}