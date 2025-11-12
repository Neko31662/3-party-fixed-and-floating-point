template <typename ShareType>
void ADDshare_share_from(const int secret_holder_id, const int party_id, std::vector<PRGSync> &PRGs,
                         NetIOMP &netio, ADDshare<ShareType> *s, ShareType x) {
#ifdef DEBUG_MODE
    s->has_shared = true;
#endif
    if (secret_holder_id == 0) {
        if (party_id == 0 || party_id == 1) {
            PRGs[0].gen_random_data(&s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
        if (party_id == 0) {
            ShareType temp = (x - s->v) & s->MASK;
            netio.send_data(2, &temp, s->BYTELEN);
        } else if (party_id == 2) {
            netio.recv_data(0, &s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
    } else {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id || party_id == 0) {
            int prg_idx = (party_id == 0) ? (secret_holder_id - 1) : 0;
            PRGs[prg_idx].gen_random_data(&s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
        if (party_id == secret_holder_id) {
            ShareType temp = (x - s->v) & s->MASK;
            netio.send_data(other_party, &temp, s->BYTELEN);
        } else if (party_id == other_party) {
            netio.recv_data(secret_holder_id, &s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
    }
}

template <typename ShareType>
void ADDshare_share_from_store(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                               ADDshare<ShareType> *s, ShareType x) {
#ifdef DEBUG_MODE
    s->has_shared = true;
#endif
    if (party_id == 0 || party_id == 1) {
        PRGs[0].gen_random_data(&s->v, s->BYTELEN);
        s->v &= s->MASK;
    }
    if (party_id == 0) {
        ShareType temp = (x - s->v) & s->MASK;
        netio.store_data(2, &temp, s->BYTELEN);
    } else if (party_id == 2) {
        netio.recv_data(0, &s->v, s->BYTELEN);
        s->v &= s->MASK;
    }
}

template <typename ShareType>
void ADDshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 ADDshare_mul_res<ShareType> *res) {
#ifdef DEBUG_MODE
    res->has_preprocess = true;
#endif
    ShareType tmpx, tmpy, tmpz;
    if (party_id == 0 || party_id == 1) {
        // P0和P1同步生成x,y,z的P1持有的份额
        PRGs[0].gen_random_data(&res->x, res->BYTELEN);
        PRGs[0].gen_random_data(&res->y, res->BYTELEN);
        PRGs[0].gen_random_data(&res->z, res->BYTELEN);
        res->x &= res->MASK;
        res->y &= res->MASK;
        res->z &= res->MASK;
        if (party_id == 0) {
            tmpx = res->x;
            tmpy = res->y;
            tmpz = res->z;
        }
    }

    if (party_id == 0 || party_id == 2) {
        // P0和P2同步生成x,y的P2持有的份额
        PRGs[(party_id + 1) % 3].gen_random_data(&res->x, res->BYTELEN);
        PRGs[(party_id + 1) % 3].gen_random_data(&res->y, res->BYTELEN);
        res->x &= res->MASK;
        res->y &= res->MASK;
    }

    if (party_id == 0) {
        ShareType temp = (tmpx + res->x) * (tmpy + res->y);
        temp -= res->z;
        temp &= res->MASK;
        netio.store_data(2, &temp, res->BYTELEN);
        // 之后需要调用send_data将temp发送给P2
    } else if (party_id == 2) {
        netio.recv_data(0, &res->z, res->BYTELEN);
        res->z &= res->MASK;
    }
}

template <typename ShareType>
void ADDshare_mul_res_cal_mult(const int party_id, NetIOMP &netio, ADDshare_mul_res<ShareType> *res,
                               ADDshare<ShareType> *s1, ADDshare<ShareType> *s2) {
#ifdef DEBUG_MODE
    if (!s1->has_shared || !s2->has_shared) {
        error("ADDshare_mul_res_cal_mult requires both inputs to have been shared");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    ShareType d = (s1->v - res->x) & res->MASK;
    ShareType e = (s2->v - res->y) & res->MASK;

    // P1和P2重构d和e，P0不参与
    ShareType d_sum, e_sum;
    if (party_id == 1) {
        netio.send_data(2, &d, res->BYTELEN);
        netio.send_data(2, &e, res->BYTELEN);

        netio.recv_data(2, &d_sum, res->BYTELEN);
        netio.recv_data(2, &e_sum, res->BYTELEN);
    } else if (party_id == 2) {
        netio.recv_data(1, &d_sum, res->BYTELEN);
        netio.recv_data(1, &e_sum, res->BYTELEN);

        netio.send_data(1, &d, res->BYTELEN);
        netio.send_data(1, &e, res->BYTELEN);
    }
    d_sum = (d_sum + d) & res->MASK;
    e_sum = (e_sum + e) & res->MASK;

    res->v = (res->z + (d_sum * res->y) + (e_sum * res->x)) & res->MASK;
    if (party_id == 1) {
        res->v = (res->v + (d_sum * e_sum)) & res->MASK;
    }
}

template <typename ShareType>
void ADDshare_mul_res_cal_mult_vec(const int party_id, NetIOMP &netio,
                                   const std::vector<ADDshare_mul_res<ShareType> *> &res,
                                   const std::vector<ADDshare<ShareType> *> &s1,
                                   const std::vector<ADDshare<ShareType> *> &s2) {
    int len = res.size();
#ifdef DEBUG_MODE
    if (s1.size() != len || s2.size() != len) {
        error("ADDshare_mul_res_cal_mult_vec: input vector sizes do not match");
    }
    for (int i = 0; i < len; i++) {
        if (!s1[i]->has_shared || !s2[i]->has_shared) {
            error("ADDshare_mul_res_cal_mult_vec requires both inputs to have been shared");
        }
        res[i]->has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }
    int ell = res[0]->BITLEN;

    std::vector<ShareType> d(len), e(len);
    for (int i = 0; i < len; i++) {
        d[i] = (s1[i]->v - res[i]->x) & res[i]->MASK;
        e[i] = (s2[i]->v - res[i]->y) & res[i]->MASK;
    }
    std::vector<ShareType> d_sum(len), e_sum(len);

    // 发送和接收信息
    encoded_msg msg1, msg2;
    for (int i = 0; i < len; i++) {
        msg1.add_msg((char *)&d[i], ell);
        msg1.add_msg((char *)&e[i], ell);
    }
    std::vector<char> buffer(msg1.get_bytelen());
    std::vector<char> buffer_recv(msg1.get_bytelen());
    msg1.to_char_array(buffer.data());

    if (party_id == 1) {
        netio.send_data(2, buffer.data(), buffer.size());
        netio.recv_data(2, buffer_recv.data(), buffer.size());
    } else if (party_id == 2) {
        netio.recv_data(1, buffer_recv.data(), buffer.size());
        netio.send_data(1, buffer.data(), buffer.size());
    }
    msg2.from_char_array(buffer_recv.data(), buffer_recv.size());
    for (int i = 0; i < len; i++) {
        ShareType tmp_d, tmp_e;
        msg2.read_msg((char *)&tmp_d, ell);
        msg2.read_msg((char *)&tmp_e, ell);
        d_sum[i] = (tmp_d + d[i]) & res[i]->MASK;
        e_sum[i] = (tmp_e + e[i]) & res[i]->MASK;

        res[i]->v = (res[i]->z + (d_sum[i] * res[i]->y) + (e_sum[i] * res[i]->x)) & res[i]->MASK;
        if (party_id == 1) {
            res[i]->v = (res[i]->v + (d_sum[i] * e_sum[i])) & res[i]->MASK;
        }
    }
}

template <typename ShareType>
ShareType ADDshare_recon(const int party_id, NetIOMP &netio, ADDshare<ShareType> *s) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("ADDshare_recon requires the input to have been shared");
    }
#endif
    if (party_id == 0) {
        return 0;
    }
    ShareType total;
    if (party_id == 1) {
        netio.send_data(2, &s->v, s->BYTELEN);
        netio.recv_data(2, &total, s->BYTELEN);
    } else if (party_id == 2) {
        netio.recv_data(1, &total, s->BYTELEN);
        netio.send_data(1, &s->v, s->BYTELEN);
    }
    total = (total + s->v) & s->MASK;
    return total;
}

template <typename ShareType>
inline void ADDshare_from_p(ADDshare<ShareType> *to, ADDshare_p *from) {
#ifdef DEBUG_MODE
    if (from->p != to->MASK + 1) {
        error("ADDshare_from_p: modulus mismatch");
    }
    to->has_shared = from->has_shared;
#endif
    to->v = from->v;
}

template <typename ShareType>
inline void ADDshare_mul_res_from_p(ADDshare_mul_res<ShareType> *to, ADDshare_p_mul_res *from) {
#ifdef DEBUG_MODE
    if (from->p != to->MASK + 1) {
        error("ADDshare_mul_res_from_p: modulus mismatch");
    }
    to->has_preprocess = from->has_preprocess;
    to->has_shared = from->has_shared;
#endif
    to->v = from->v;
    to->x = from->x;
    to->y = from->y;
    to->z = from->z;
}