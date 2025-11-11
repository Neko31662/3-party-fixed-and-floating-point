#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"
template <int lf, int le> class FLTshare {
  public:
    // value = (-1)^b * t * 2^(e - 2^(le-1))
    MSSshare<1> b;
    MSSshare<lf + 2> t;
    MSSshare<le> e;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    bool has_shared = false;
#endif
};

class FLTplain {
  public:
    ShareValue b; // 0 or 1
    ShareValue t; // lf + 2 bits
    ShareValue e; // le bits
};

// ===============================================================
// ======================= implementation ========================
// ===============================================================

template <int lf, int le>
inline void FLTshare_preprocess(const int secret_holder_id, const int party_id,
                                std::vector<PRGSync> &PRGs, NetIOMP &netio, FLTshare<lf, le> *x) {
#ifdef DEBUG_MODE
    if (x->has_preprocess) {
        error("FLTshare_preprocess has already been called on this object");
    }
    x->has_preprocess = true;
#endif
    MSSshare_preprocess(secret_holder_id, party_id, PRGs, netio, &x->b);
    MSSshare_preprocess(secret_holder_id, party_id, PRGs, netio, &x->t);
    MSSshare_preprocess(secret_holder_id, party_id, PRGs, netio, &x->e);
}

template <int lf, int le>
inline void FLTshare_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                                FLTshare<lf, le> *x, bool b, ShareValue t, ShareValue e) {
#ifdef DEBUG_MODE
    if (!x->has_preprocess) {
        error("FLTshare_share_from: x must be preprocessed before calling FLTshare_share_from");
    }
    x->has_shared = true;
#endif
    MSSshare_share_from(secret_holder_id, party_id, netio, &x->b, b);
    MSSshare_share_from(secret_holder_id, party_id, netio, &x->t, t);
    MSSshare_share_from(secret_holder_id, party_id, netio, &x->e, e);
}

template <int lf, int le>
inline void FLTshare_share_from_store(const int party_id, NetIOMP &netio, FLTshare<lf, le> *x,
                                      bool b, ShareValue t, ShareValue e) {
#ifdef DEBUG_MODE
    if (!x->has_preprocess) {
        error("FLTshare_share_from_store: x must be preprocessed before calling "
              "FLTshare_share_from_store");
    }
    x->has_shared = true;
#endif
    MSSshare_share_from_store(party_id, netio, &x->b, b);
    MSSshare_share_from_store(party_id, netio, &x->t, t);
    MSSshare_share_from_store(party_id, netio, &x->e, e);
}

template <int lf, int le>
inline FLTplain FLTshare_recon(const int party_id, NetIOMP &netio, FLTshare<lf, le> *s) {
    FLTplain result;
    result.b = MSSshare_recon(party_id, netio, &s->b);
    result.t = MSSshare_recon(party_id, netio, &s->t);
    result.e = MSSshare_recon(party_id, netio, &s->e);
    return result;
}
