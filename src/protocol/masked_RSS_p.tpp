
ShareValue MSSshare_p_recon(const int party_id, NetIOMP &netio, MSSshare_p *s) {
    ShareValue total(0);
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
    total = (total + s->v1 + s->v2) % s->p;
    return total;
}

void MSSshare_p_preprocess(const int secret_holder_id, const int party_id,
                           std::vector<PRGSync> &PRGs, NetIOMP &netio, MSSshare_p *s) {
#ifdef DEBUG_MODE
    if (s->has_preprocess) {
        error("MSSshare_p_preprocess has already been called on this object");
    }
    s->has_preprocess = true;
#endif

    if (secret_holder_id != 2) {
        if (party_id == 0) {
            PRGs[0].gen_random_data(&s->v1, s->BYTELEN);
            s->v1 %= s->p;
        } else if (party_id == 1) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 %= s->p;
        }
    }

    if (secret_holder_id != 1) {
        if (party_id == 0) {
            PRGs[1].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 %= s->p;
        } else if (party_id == 2) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 %= s->p;
        }
    }
}

void MSSshare_p_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                           MSSshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_p_share_from must be invoked after MSSshare_p_preprocess");
    }
    s->has_shared = true;
#endif

    // Party 0 holds secret x
    if (secret_holder_id == 0) {
        if (party_id == 0) {
            ShareValue m = x;
            m = (m - s->v1 + s->p) % s->p;
            m = (m - s->v2 + s->p) % s->p;
            // send m = x - r1 - r2
            netio.send_data(1, &m, s->BYTELEN);
            netio.send_data(2, &m, s->BYTELEN);
        } else {
            s->v1 = 0;
            netio.recv_data(0, &s->v1, s->BYTELEN);
        }
    }
    // Party 1,2 holds secret x
    else if (secret_holder_id == 1 || secret_holder_id == 2) {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id) {
            s->v1 = x;
            s->v1 = (s->v1 - s->v2 + s->p) % s->p;
            // send m = x - r1(or r2)
            netio.send_data(other_party, &s->v1, s->BYTELEN);
        } else if (party_id == other_party) {
            s->v1 = 0;
            netio.recv_data(secret_holder_id, &s->v1, s->BYTELEN);
        }
    }
}

void MSSshare_p_share_from_store(const int party_id, NetIOMP &netio, MSSshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_p_share_from must be invoked after MSSshare_p_preprocess");
    }
    s->has_shared = true;
#endif

    if (party_id == 0) {
        ShareValue m = x;
        m = (m - s->v1 + s->p) % s->p;
        m = (m - s->v2 + s->p) % s->p;
        // send m = x - r1 - r2
        netio.send_data(1, &m, s->BYTELEN); // P0向P1发送的不store
        netio.store_data(2, &m, s->BYTELEN);
    } else {
        s->v1 = 0;
        netio.recv_data(0, &s->v1, s->BYTELEN);
    }
}

void MSSshare_p_add_plain(const int party_id, MSSshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("MSSshare_p_add_plain must be invoked after shared");
    }
#endif
    if (party_id != 0) {
        s->v1 = (s->v1 + x) % s->p;
    }
}

void MSSshare_p_add_res_preprocess(const int party_id, MSSshare_p *res, MSSshare_p *s1,
                                   MSSshare_p *s2) {
    auto s_vec = make_ptr_vec(*s1, *s2);
    auto coeff_vec = std::vector<int>{1, 1};
    MSSshare_p_add_res_preprocess_multi(party_id, res, s_vec, coeff_vec);
}

void MSSshare_p_add_res_preprocess_multi(const int party_id, MSSshare_p *res,
                                         std::vector<MSSshare_p *> &s_vec,
                                         std::vector<int> &coeff_vec) {
#ifdef DEBUG_MODE
    for (size_t i = 0; i < s_vec.size(); i++) {
        if (!s_vec[i]->has_preprocess) {
            error("MSSshare_p_add_res_preprocess_multi: all shares must have been preprocessed");
        }
        if (res->p != s_vec[i]->p) {
            error("MSSshare_p_add_res_preprocess_multi: modulus p mismatch");
        }
    }
    if (res->has_preprocess) {
        error("MSSshare_p_add_res_preprocess_multi has already been called on this object");
    }
    res->has_preprocess = true;
#endif

    res->v1 = 0;
    res->v2 = 0;
    for (size_t i = 0; i < s_vec.size(); i++) {
        int coeff_mod;
        if (coeff_vec[i] >= 0) {
            coeff_mod = coeff_vec[i] % res->p;
        } else {
            int tmp = (-coeff_vec[i]) % res->p;
            coeff_mod = (res->p - tmp) % res->p;
        }
        res->v1 = (res->v1 + coeff_mod * s_vec[i]->v1) % res->p;
        res->v2 = (res->v2 + coeff_mod * s_vec[i]->v2) % res->p;
    }
}

void MSSshare_p_add_res_calc_add(const int party_id, MSSshare_p *res, MSSshare_p *s1,
                                 MSSshare_p *s2) {
    auto s_vec = make_ptr_vec(*s1, *s2);
    auto coeff_vec = std::vector<int>{1, 1};
    MSSshare_p_add_res_calc_add_multi(party_id, res, s_vec, coeff_vec);
}

void MSSshare_p_add_res_calc_add_multi(const int party_id, MSSshare_p *res,
                                       std::vector<MSSshare_p *> &s_vec,
                                       std::vector<int> &coeff_vec) {
#ifdef DEBUG_MODE
    for (size_t i = 0; i < s_vec.size(); i++) {
        if (!s_vec[i]->has_shared) {
            error("MSSshare_p_add_res_calc_add_multi requires all inputs to have been shared");
        }
        if (res->p != s_vec[i]->p) {
            error("MSSshare_p_add_res_calc_add_multi: modulus p mismatch");
        }
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }
    res->v1 = 0;
    for (size_t i = 0; i < s_vec.size(); i++) {
        int coeff_mod;
        if (coeff_vec[i] >= 0) {
            coeff_mod = coeff_vec[i] % res->p;
        } else {
            int tmp = (-coeff_vec[i]) % res->p;
            coeff_mod = (res->p - tmp) % res->p;
        }
        res->v1 = (res->v1 + coeff_mod * s_vec[i]->v1) % res->p;
    }
}

void MSSshare_p_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                   MSSshare_p_mul_res *res, const MSSshare_p *s1,
                                   const MSSshare_p *s2) {

#ifdef DEBUG_MODE
    if (!s1->has_preprocess || !s2->has_preprocess) {
        error("MSSshare_p_mul_res_preprocess requires s1 and s2 to have been preprocessed");
    }
    if (res->p != s1->p || res->p != s2->p) {
        error("MSSshare_p_add_res_preprocess: modulus p mismatch");
    }
    if (res->has_preprocess) {
        error("MSSshare_p_mul_res_preprocess has already been called on this object");
    }
#endif

    MSSshare_p_preprocess(0, party_id, PRGs, netio, res);

    if (party_id == 0 || party_id == 1) {
        PRGs[0].gen_random_data(&res->v3, res->BYTELEN);
        res->v3 %= res->p;
    }
    if (party_id == 0) {
        ShareValue temp = (s1->v1 + s1->v2) % s1->p;
        temp = (temp * ((s2->v1 + s2->v2) % s2->p)) % res->p;
        temp = (temp - res->v3 + res->p) % res->p;
        netio.store_data(2, &temp, res->BYTELEN);
        // need to send later
    } else if (party_id == 2) {
        res->v3 = 0;
        netio.recv_data(0, &res->v3, res->BYTELEN);
    }
}

void MSSshare_p_mul_res_calc_mul(const int party_id, NetIOMP &netio, MSSshare_p_mul_res *res,
                                 const MSSshare_p *s1, const MSSshare_p *s2) {
#ifdef DEBUG_MODE
    if (!res->has_preprocess) {
        error("MSSshare_p_mul_res_calc_mul must be invoked after "
              "MSSshare_p_mul_res_preprocess");
    }
    if (!s1->has_shared || !s2->has_shared) {
        error("MSSshare_p_mul_res_calc_mul requires s1 and s2 to have been shared");
    }
    if (res->p != s1->p || res->p != s2->p) {
        error("MSSshare_p_add_res_preprocess: modulus p mismatch");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    ShareValue temp = 0;
    ShareValue recv_temp = 0;
    temp = (temp + (s1->v1 * s2->v2) % res->p) % res->p;
    temp = (temp + (s1->v2 * s2->v1) % res->p) % res->p;
    temp = (temp + res->v3) % res->p;
    temp = (temp - res->v2 + res->p) % res->p;

    if (party_id == 1) {
        temp = (temp + (s1->v1 * s2->v1) % res->p) % res->p;
        netio.send_data(2, &temp, res->BYTELEN);

        netio.recv_data(2, &recv_temp, res->BYTELEN);
    } else if (party_id == 2) {
        netio.recv_data(1, &recv_temp, res->BYTELEN);
        netio.send_data(1, &temp, res->BYTELEN);
    }
    res->v1 = (recv_temp + temp) % res->p;
}

inline void MSSshare_p_mul_res_calc_mul_vec(const int party_id, NetIOMP &netio,
                                            std::vector<MSSshare_p_mul_res *> &res_vec,
                                            std::vector<MSSshare_p *> &s1_vec,
                                            std::vector<MSSshare_p *> &s2_vec) {
#ifdef DEBUG_MODE
    if (res_vec.size() != s1_vec.size() || res_vec.size() != s2_vec.size()) {
        error("MSSshare_p_mul_res_calc_mul_vec: input vector size mismatch");
    }
    for (size_t i = 0; i < res_vec.size(); i++) {
        MSSshare_p_mul_res *res = res_vec[i];
        MSSshare_p *s1 = s1_vec[i];
        MSSshare_p *s2 = s2_vec[i];
        if (!res->has_preprocess) {
            error("MSSshare_p_mul_res_calc_mul must be invoked after "
                  "MSSshare_p_mul_res_preprocess");
        }
        if (!s1->has_shared || !s2->has_shared) {
            error("MSSshare_p_mul_res_calc_mul requires s1 and s2 to have been shared");
        }
        if (res->p != s1->p || res->p != s2->p) {
            error("MSSshare_p_add_res_preprocess: modulus p mismatch");
        }
        res->has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }

    int vec_len = res_vec.size();
    int total_bitp = 0;
    for (size_t i = 0; i < vec_len; i++) {
        MSSshare_p_mul_res *res = res_vec[i];
        MSSshare_p *s1 = s1_vec[i];
        MSSshare_p *s2 = s2_vec[i];
        total_bitp += res->BITLEN;
    }
    int total_bytep = (total_bitp + 7) / 8;
    encoded_msg msg, msg_recv;

    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        auto &s1 = s1_vec[i];
        auto &s2 = s2_vec[i];

        ShareValue temp = 0;
        temp = (temp + (s1->v1 * s2->v2) % res->p) % res->p;
        temp = (temp + (s1->v2 * s2->v1) % res->p) % res->p;
        temp = (temp + res->v3) % res->p;
        temp = (temp - res->v2 + res->p) % res->p;

        if (party_id == 1) {
            temp += s1->v1 * s2->v1;
            temp %= res->p;
        }
        msg.add_msg((char *)&temp, res->BITLEN);
        res->v1 = temp % res->p;
    }

    if (party_id == 1) {
        std::vector<char> tmp(total_bytep);
        msg.to_char_array(tmp.data());
        netio.send_data(2, tmp.data(), total_bytep);

        netio.recv_data(2, tmp.data(), total_bytep);
        msg_recv.from_char_array(tmp.data(), total_bytep);
    } else if (party_id == 2) {
        std::vector<char> tmp(total_bytep);
        netio.recv_data(1, tmp.data(), total_bytep);
        msg_recv.from_char_array(tmp.data(), total_bytep);

        msg.to_char_array(tmp.data());
        netio.send_data(1, tmp.data(), total_bytep);
    }

    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        ShareValue recv_temp = 0;
        msg_recv.read_msg((char *)&recv_temp, res->BITLEN);
        res->v1 = (recv_temp + res->v1) % res->p;
    }
}
