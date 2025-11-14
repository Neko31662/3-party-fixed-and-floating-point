ShareValue MSSshare_recon(const int party_id, NetIOMP &netio, MSSshare *s) {
    ShareValue total;
    if (party_id == 0) {
        netio.send_data(1, &s->v2, s->BYTELEN);
        netio.send_data(2, &s->v1, s->BYTELEN);

        netio.recv_data(1, &total, s->BYTELEN);
    } else if (party_id == 1) {
        netio.recv_data(0, &total, s->BYTELEN);
        netio.send_data(0, &s->v1, s->BYTELEN);

    } else if (party_id == 2) {
        netio.recv_data(0, &total, s->BYTELEN);
    }
    total = (total + s->v1 + s->v2) & s->MASK;
    return total;
}

void MSSshare_preprocess(const int secret_holder_id, const int party_id, std::vector<PRGSync> &PRGs,
                         NetIOMP &netio, MSSshare *s) {
#ifdef DEBUG_MODE
    s->has_preprocess = true;
#endif

    if (secret_holder_id != 2) {
        if (party_id == 0) {
            PRGs[0].gen_random_data(&s->v1, s->BYTELEN);
            s->v1 &= s->MASK;
        } else if (party_id == 1) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 &= s->MASK;
        }
    }

    if (secret_holder_id != 1) {
        if (party_id == 0) {
            PRGs[1].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 &= s->MASK;
        } else if (party_id == 2) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 &= s->MASK;
        }
    }
}

void MSSshare_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                         MSSshare *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_share_from must be invoked after MSSshare_preprocess");
    }
    s->has_shared = true;
#endif

    // Party 0 holds secret x
    if (secret_holder_id == 0) {
        if (party_id == 0) {
            ShareValue m = x;
            m -= s->v1;
            m -= s->v2;
            m &= s->MASK;
            // send m = x - r1 - r2
            netio.send_data(1, &m, s->BYTELEN);
            netio.send_data(2, &m, s->BYTELEN);
        } else {
            netio.recv_data(0, &s->v1, s->BYTELEN);
            s->v1 &= s->MASK;
        }
    }
    // Party 1,2 holds secret x
    else if (secret_holder_id == 1 || secret_holder_id == 2) {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id) {
            s->v1 = x;
            s->v1 -= s->v2;
            s->v1 &= s->MASK;
            // send m = x - r1(or r2)
            netio.send_data(other_party, &s->v1, s->BYTELEN);
        } else if (party_id == other_party) {
            netio.recv_data(secret_holder_id, &s->v1, s->BYTELEN);
            s->v1 &= s->MASK;
        }
    }
}

void MSSshare_share_from_store(const int party_id, NetIOMP &netio, MSSshare *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_share_from must be invoked after MSSshare_preprocess");
    }
    s->has_shared = true;
#endif

    if (party_id == 0) {
        ShareValue m = x;
        m -= s->v1;
        m -= s->v2;
        m &= s->MASK;
        // send m = x - r1 - r2
        netio.send_data(1, &m, s->BYTELEN); // P0向P1发送的不store
        netio.store_data(2, &m, s->BYTELEN);
    } else {
        netio.recv_data(0, &s->v1, s->BYTELEN);
        s->v1 &= s->MASK;
    }
}

void MSSshare_add_plain(const int party_id, MSSshare *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("MSSshare_add_plain must be invoked after shared");
    }
#endif
    if (party_id != 0) {
        s->v1 = (s->v1 + x) & s->MASK;
    }
}

void MSSshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 MSSshare_mul_res *res, const MSSshare *s1, const MSSshare *s2) {

#ifdef DEBUG_MODE
    if (!s1->has_preprocess || !s2->has_preprocess) {
        error("MSSshare_mul_res_preprocess requires s1 and s2 to have been preprocessed");
    }
    if (res->BITLEN != s1->BITLEN || res->BITLEN != s2->BITLEN) {
        error("MSSshare_mul_res_preprocess: bit-length mismatch");
    }
    res->has_preprocess = true;
#endif

    MSSshare_preprocess(0, party_id, PRGs, netio, res);

    if (party_id == 0 || party_id == 1) {
        PRGs[0].gen_random_data(&res->v3, res->BYTELEN);
        res->v3 &= res->MASK;
    }
    if (party_id == 0) {
        ShareValue temp = (s1->v1 + s1->v2) * (s2->v1 + s2->v2);
        temp -= res->v3;
        temp &= res->MASK;
        netio.store_data(2, &temp, res->BYTELEN);
        // need to send later
    } else if (party_id == 2) {
        netio.recv_data(0, &res->v3, res->BYTELEN);
        res->v3 &= res->MASK;
    }
}

void MSSshare_mul_res_calc_mul(const int party_id, NetIOMP &netio, MSSshare_mul_res *res,
                               const MSSshare *s1, const MSSshare *s2) {
#ifdef DEBUG_MODE
    if (!res->has_preprocess) {
        error("MSSshare_mul_res_calc_mul must be invoked after "
              "MSSshare_mul_res_preprocess");
    }
    if (!s1->has_shared || !s2->has_shared) {
        error("MSSshare_mul_res_calc_mul requires s1 and s2 to have been shared");
    }
    if (res->BITLEN != s1->BITLEN || res->BITLEN != s2->BITLEN) {
        error("MSSshare_mul_res_calc_mul: bit-length mismatch");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }
    ShareValue temp = 0;
    ShareValue recv_temp;
    temp += s1->v1 * s2->v2;
    temp += s1->v2 * s2->v1;
    temp += res->v3;
    temp -= res->v2;
    if (party_id == 1) {
        temp += s1->v1 * s2->v1;
    }

    netio.send_data(3 - party_id, &temp, res->BYTELEN);
    netio.recv_data(3 - party_id, &recv_temp, res->BYTELEN);

    res->v1 = (recv_temp + temp) & res->MASK;
}

inline void MSSshare_mul_res_calc_mul_vec(const int party_id, NetIOMP &netio,
                                          std::vector<MSSshare_mul_res *> &res_vec,
                                          std::vector<MSSshare *> &s1_vec,
                                          std::vector<MSSshare *> &s2_vec) {
#ifdef DEBUG_MODE
    if (res_vec.size() != s1_vec.size() || res_vec.size() != s2_vec.size()) {
        error("MSSshare_mul_res_calc_mul_vec: size of res_vec, s1_vec and s2_vec must be equal");
    }
    for (size_t i = 0; i < res_vec.size(); i++) {
        auto &res = res_vec[i];
        auto &s1 = s1_vec[i];
        auto &s2 = s2_vec[i];
        if (!res->has_preprocess) {
            error("MSSshare_mul_res_calc_mul_vec must be invoked after "
                  "MSSshare_mul_res_preprocess");
        }
        if (!s1->has_shared || !s2->has_shared) {
            error("MSSshare_mul_res_calc_mul_vec requires s1 and s2 to have been shared");
        }
        if (res->BITLEN != s1->BITLEN || res->BITLEN != s2->BITLEN) {
            error("MSSshare_mul_res_calc_mul_vec: bit-length mismatch");
        }
        res->has_shared = true;
    }
#endif

    if (party_id == 0) {
        return;
    }

    int vec_len = res_vec.size();

    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        auto &s1 = s1_vec[i];
        auto &s2 = s2_vec[i];

        ShareValue temp = 0;
        temp += s1->v1 * s2->v2;
        temp += s1->v2 * s2->v1;
        temp += res->v3;
        temp -= res->v2;

        if (party_id == 1) {
            temp += s1->v1 * s2->v1;
        }
        res->v1 = temp & res->MASK;
        netio.send_data(3 - party_id, &temp, res->BYTELEN);
    }

    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        ShareValue recv_temp = 0;
        netio.recv_data(3 - party_id, &recv_temp, res->BYTELEN);
        res->v1 = (recv_temp + res->v1) & res->MASK;
    }
}

inline void MSSshare_from_p(MSSshare *to, MSSshare_p *from) {
#ifdef DEBUG_MODE
    if (from->p != to->MASK + 1) {
        error("MSSshare_from_p: modulus mismatch");
    }
    to->has_preprocess = from->has_preprocess;
    to->has_shared = from->has_shared;
#endif
    to->v1 = from->v1;
    to->v2 = from->v2;
}

inline void MSSshare_mul_res_from_p(MSSshare_mul_res *to, MSSshare_p_mul_res *from) {
#ifdef DEBUG_MODE
    if (from->p != to->MASK + 1) {
        error("MSSshare_mul_res_from_p: modulus mismatch");
    }
    to->has_preprocess = from->has_preprocess;
    to->has_shared = from->has_shared;
#endif
    to->v1 = from->v1;
    to->v2 = from->v2;
    to->v3 = from->v3;
}