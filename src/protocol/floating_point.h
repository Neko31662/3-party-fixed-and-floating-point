#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"

class FLTshare {
  public:
    int lf;
    int le;
    // value = (-1)^b * t * 2^(e - 2^(le-1))
    MSSshare b;
    MSSshare t;
    MSSshare e;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    bool has_shared = false;
#endif

    FLTshare(int lf, int le) : b(1), t(lf + 2), e(le) {
        this->lf = lf;
        this->le = le;
    }
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

inline void FLTshare_preprocess(const int secret_holder_id, const int party_id,
                                std::vector<PRGSync> &PRGs, NetIOMP &netio, FLTshare *x) {
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

inline void FLTshare_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                                FLTshare *x, bool b, ShareValue t, ShareValue e) {
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

inline void FLTshare_share_from_store(const int party_id, NetIOMP &netio, FLTshare *x, bool b,
                                      ShareValue t, ShareValue e) {
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

inline FLTplain FLTshare_recon(const int party_id, NetIOMP &netio, FLTshare *s) {
    FLTplain result;
    result.b = MSSshare_recon(party_id, netio, &s->b);
    result.t = MSSshare_recon(party_id, netio, &s->t);
    result.e = MSSshare_recon(party_id, netio, &s->e);
    return result;
}
