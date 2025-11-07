template <int ell>
void ADDshare_share_from(const int secret_holder_id, const int party_id, std::vector<PRGSync> &PRGs,
                         NetIOMP &netio, ADDshare<ell> *s, LongShareValue x) {
#ifdef DEBUG_MODE
    s->has_shared = true;
#endif
    if (secret_holder_id == 0) {
        if (party_id == 0 || party_id == 1) {
            PRGs[0].gen_random_data(&s->v, ADDshare<ell>::BYTELEN);
            s->v &= ADDshare<ell>::MASK;
        }
        if (party_id == 0) {
            LongShareValue temp = (x - s->v) & ADDshare<ell>::MASK;
            netio.send_data(2, &temp, ADDshare<ell>::BYTELEN);
        } else if (party_id == 2) {
            netio.recv_data(0, &s->v, ADDshare<ell>::BYTELEN);
            s->v &= ADDshare<ell>::MASK;
        }
    } else {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id || party_id == 0) {
            int prg_idx = (party_id == 0) ? (secret_holder_id - 1) : 0;
            PRGs[prg_idx].gen_random_data(&s->v, ADDshare<ell>::BYTELEN);
            s->v &= ADDshare<ell>::MASK;
        }
        if (party_id == secret_holder_id) {
            LongShareValue temp = (x - s->v) & ADDshare<ell>::MASK;
            netio.send_data(other_party, &temp, ADDshare<ell>::BYTELEN);
        } else if (party_id == other_party) {
            netio.recv_data(secret_holder_id, &s->v, ADDshare<ell>::BYTELEN);
            s->v &= ADDshare<ell>::MASK;
        }
    }
}

template <int ell>
void ADDshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 ADDshare_mul_res<ell> *res) {
#ifdef DEBUG_MODE
    res->has_preprocess = true;
#endif
    LongShareValue tmpx, tmpy, tmpz;
    if (party_id == 0 || party_id == 1) {
        // P0和P1同步生成x,y,z的P1持有的份额
        PRGs[0].gen_random_data(&res->x, ADDshare<ell>::BYTELEN);
        PRGs[0].gen_random_data(&res->y, ADDshare<ell>::BYTELEN);
        PRGs[0].gen_random_data(&res->z, ADDshare<ell>::BYTELEN);
        res->x &= ADDshare<ell>::MASK;
        res->y &= ADDshare<ell>::MASK;
        res->z &= ADDshare<ell>::MASK;
        if (party_id == 0) {
            tmpx = res->x;
            tmpy = res->y;
            tmpz = res->z;
        }
    }

    if (party_id == 0 || party_id == 2) {
        // P0和P2同步生成x,y的P2持有的份额
        PRGs[(party_id + 1) % 3].gen_random_data(&res->x, ADDshare<ell>::BYTELEN);
        PRGs[(party_id + 1) % 3].gen_random_data(&res->y, ADDshare<ell>::BYTELEN);
        res->x &= ADDshare<ell>::MASK;
        res->y &= ADDshare<ell>::MASK;
    }

    if (party_id == 0) {
        LongShareValue temp = (tmpx + res->x) * (tmpy + res->y);
        temp -= res->z;
        temp &= ADDshare<ell>::MASK;
        netio.store_data(2, &temp, ADDshare<ell>::BYTELEN);
        // 之后需要调用send_data将temp发送给P2
    } else if (party_id == 2) {
        netio.recv_data(0, &res->z, ADDshare<ell>::BYTELEN);
        res->z &= ADDshare<ell>::MASK;
    }
}

template <int ell>
void ADDshare_mul_res_cal_mult(const int party_id, NetIOMP &netio, ADDshare_mul_res<ell> *res,
                               ADDshare<ell> *s1, ADDshare<ell> *s2) {
#ifdef DEBUG_MODE
    if (!s1->has_shared || !s2->has_shared) {
        error("ADDshare_mul_res_cal_mult requires both inputs to have been shared");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    LongShareValue d = (s1->v - res->x) & ADDshare<ell>::MASK;
    LongShareValue e = (s2->v - res->y) & ADDshare<ell>::MASK;

    // P1和P2重构d和e，P0不参与
    LongShareValue d_sum, e_sum;
    if (party_id == 1) {
        netio.send_data(2, &d, ADDshare<ell>::BYTELEN);
        netio.send_data(2, &e, ADDshare<ell>::BYTELEN);

        netio.recv_data(2, &d_sum, ADDshare<ell>::BYTELEN);
        netio.recv_data(2, &e_sum, ADDshare<ell>::BYTELEN);
    } else if (party_id == 2) {
        netio.recv_data(1, &d_sum, ADDshare<ell>::BYTELEN);
        netio.recv_data(1, &e_sum, ADDshare<ell>::BYTELEN);

        netio.send_data(1, &d, ADDshare<ell>::BYTELEN);
        netio.send_data(1, &e, ADDshare<ell>::BYTELEN);
    }
    d_sum = (d_sum + d) & ADDshare<ell>::MASK;
    e_sum = (e_sum + e) & ADDshare<ell>::MASK;

    res->v = (res->z + (d_sum * res->y) + (e_sum * res->x)) & ADDshare<ell>::MASK;
    if (party_id == 1) {
        res->v = (res->v + (d_sum * e_sum)) & ADDshare<ell>::MASK;
    }
}

template <int ell>
void ADDshare_mul_res_cal_mult_vec(const int party_id, NetIOMP &netio,
                                   const std::vector<ADDshare_mul_res<ell> *> &res,
                                   const std::vector<ADDshare<ell> *> &s1,
                                   const std::vector<ADDshare<ell> *> &s2) {
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

    std::vector<LongShareValue> d(len), e(len);
    for (int i = 0; i < len; i++) {
        d[i] = (s1[i]->v - res[i]->x) & ADDshare<ell>::MASK;
        e[i] = (s2[i]->v - res[i]->y) & ADDshare<ell>::MASK;
    }
    std::vector<LongShareValue> d_sum(len), e_sum(len);

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
        LongShareValue tmp_d, tmp_e;
        msg2.read_msg((char *)&tmp_d, ell);
        msg2.read_msg((char *)&tmp_e, ell);
        d_sum[i] = (tmp_d + d[i]) & ADDshare<ell>::MASK;
        e_sum[i] = (tmp_e + e[i]) & ADDshare<ell>::MASK;

        res[i]->v = (res[i]->z + (d_sum[i] * res[i]->y) + (e_sum[i] * res[i]->x)) & ADDshare<ell>::MASK;
        if (party_id == 1) {
            res[i]->v = (res[i]->v + (d_sum[i] * e_sum[i])) & ADDshare<ell>::MASK;
        }
    }
}

template <int ell>
LongShareValue ADDshare_recon(const int party_id, NetIOMP &netio, ADDshare<ell> *s) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("ADDshare_recon requires the input to have been shared");
    }
#endif
    if (party_id == 0) {
        return 0;
    }
    LongShareValue total;
    if (party_id == 1) {
        netio.send_data(2, &s->v, ADDshare<ell>::BYTELEN);
        netio.recv_data(2, &total, ADDshare<ell>::BYTELEN);
    } else if (party_id == 2) {
        netio.recv_data(1, &total, ADDshare<ell>::BYTELEN);
        netio.send_data(1, &s->v, ADDshare<ell>::BYTELEN);
    }
    total = (total + s->v) & ADDshare<ell>::MASK;
    return total;
}