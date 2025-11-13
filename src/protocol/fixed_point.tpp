void PI_fixed_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_fixed_mult_intermediate &PI_fixed_mult_intermediate,
                              MSSshare *input_x, MSSshare *input_y, MSSshare *output_z) {
    int li = PI_fixed_mult_intermediate.li;
    int lf = PI_fixed_mult_intermediate.lf;
    int l_res = PI_fixed_mult_intermediate.l_res;
#ifdef DEBUG_MODE
    if (!input_x->has_preprocess) {
        error("PI_fixed_mult_preprocess: input_x must be preprocessed before calling "
              "PI_fixed_mult_preprocess");
    }
    if (!input_y->has_preprocess) {
        error("PI_fixed_mult_preprocess: input_y must be preprocessed before calling "
              "PI_fixed_mult_preprocess");
    }
    if (output_z->has_preprocess) {
        error("PI_fixed_mult_preprocess: output_z has already been preprocessed");
    }
    if (PI_fixed_mult_intermediate.has_preprocess) {
        error("PI_fixed_mult_preprocess has already been called on this object");
    }
    if (input_x->BITLEN != li + lf || input_y->BITLEN != li + lf || output_z->BITLEN != l_res) {
        error("PI_fixed_mult_preprocess: input_x, input_y or output_z bitlength mismatch");
    }
    PI_fixed_mult_intermediate.has_preprocess = true;
#endif

    const int l_input = li + lf;
    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare &z = *output_z;
    ADDshare<> &Gamma = PI_fixed_mult_intermediate.Gamma;
    ADDshare<> *lf_share1 = PI_fixed_mult_intermediate.lf_share1;
    ADDshare<> *lf_share2 = PI_fixed_mult_intermediate.lf_share2;
    ADDshare<> *lf_share3 = PI_fixed_mult_intermediate.lf_share3;

    // Step 1: 将x和y视为lf + l_res位的分享
    MSSshare input_origin[2] = {x, y};
    input_origin[0].v1 &= input_origin[0].MASK;
    input_origin[0].v2 &= input_origin[0].MASK;
    input_origin[1].v1 &= input_origin[1].MASK;
    input_origin[1].v2 &= input_origin[1].MASK;

    // Step 2: Gamma += (r_x_1 + r_x_2) * (r_y_1 + r_y_2)
    if (party_id == 0) {
        Gamma.v = 0;
        Gamma.v =
            (input_origin[0].v1 + input_origin[0].v2) * (input_origin[1].v1 + input_origin[1].v2);
        Gamma.v &= Gamma.MASK;
    }

    // 对称地求f(x,y)和f(y,x)
    // i=0, a <-- x, b <-- y
    // i=1, a <-- y, b <-- x
    // 首先求出基本元素
    if (party_id == 0) {
        bool w0[2]; // r1+r2的进位
        bool r0[2]; // r的最高位
        for (int i = 0; i < 2; i++) {
            ShareValue r_a = (input_origin[i].v1 + input_origin[i].v2) & input_origin[i].MASK;
            if (r_a < input_origin[i].v1) {
                w0[i] = 1;
            } else {
                w0[i] = 0;
            }

            r0[i] = (r_a >> (l_input - 1)) & 1;
        }

        for (int i = 0; i < 2; i++) {
            // Step 3: Gamma -= 2^l_input * (w_a(0)+r_a(0)) * (r_b_1+r_b_2)
            ShareValue temp = (w0[i] + r0[i]) * ((input_origin[1 - i].v1 + input_origin[1 - i].v2));
            temp <<= l_input;
            Gamma.v = (Gamma.v - temp) & Gamma.MASK;

            // Step 4: lf_share1[i] = r_a(0); lf_share2[i] = w_a(0)+r_a(0);
            // lf_share3[i] = (1 - r_a(0)) * (r_b_1 + r_b_2)
            lf_share1[i].v = r0[i];
            lf_share2[i].v = w0[i] + r0[i];
            lf_share3[i].v = (1 - r0[i]) * (input_origin[1 - i].v1 + input_origin[1 - i].v2);
            lf_share3[i].v &= lf_share3[i].MASK;
        }
    }

    // Step 5: share all
    ADDshare_share_from_store(party_id, PRGs, netio, &Gamma, Gamma.v);
    for (int i = 0; i < 2; i++) {
        ADDshare_share_from_store(party_id, PRGs, netio, &lf_share1[i], lf_share1[i].v);
        ADDshare_share_from_store(party_id, PRGs, netio, &lf_share2[i], lf_share2[i].v);
        ADDshare_share_from_store(party_id, PRGs, netio, &lf_share3[i], lf_share3[i].v);
    }

    // Step 6: preprocess z
    MSSshare_preprocess(0, party_id, PRGs, netio, &z);
}

void PI_fixed_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_fixed_mult_intermediate &PI_fixed_mult_intermediate, MSSshare *input_x,
                   MSSshare *input_y, MSSshare *output_z, bool has_offset) {
    int li = PI_fixed_mult_intermediate.li;
    int lf = PI_fixed_mult_intermediate.lf;
    int l_res = PI_fixed_mult_intermediate.l_res;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_fixed_mult: input_x must be shared before calling PI_fixed_mult");
    }
    if (!input_y->has_shared) {
        error("PI_fixed_mult: input_y must be shared before calling PI_fixed_mult");
    }
    if (!PI_fixed_mult_intermediate.has_preprocess) {
        error("PI_fixed_mult: PI_fixed_mult_intermediate must be preprocessed before calling "
              "PI_fixed_mult");
    }
    if (!output_z->has_preprocess) {
        error("PI_fixed_mult: output_z must be preprocessed before calling PI_fixed_mult");
    }
    if (input_x->BITLEN != li + lf || input_y->BITLEN != li + lf || output_z->BITLEN != l_res) {
        error("PI_fixed_mult_preprocess: input_x, input_y or output_z bitlength mismatch");
    }
    output_z->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    const int l_input = li + lf;
    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare &z = *output_z;
    ADDshare<> &Gamma = PI_fixed_mult_intermediate.Gamma;
    ADDshare<> *lf_share1 = PI_fixed_mult_intermediate.lf_share1;
    ADDshare<> *lf_share2 = PI_fixed_mult_intermediate.lf_share2;
    ADDshare<> *lf_share3 = PI_fixed_mult_intermediate.lf_share3;

    // Step 1: 如果有offset，将x和y的m加上2^(l-2)，之后视为lf + l_res位的分享
    MSSshare input_origin[2] = {x, y};
    for (int i = 0; i < 2; i++) {
        if (has_offset) {
            input_origin[i].v1 += (ShareValue(1) << (l_input - 2));
        }
        input_origin[i].v1 &= input_origin[i].MASK;
        input_origin[i].v2 &= input_origin[i].MASK;
    }

    ADDshare<> mz_share(lf + l_res); // 结果的m值的分享
#ifdef DEBUG_MODE
    mz_share.has_shared = true;
#endif

    // Step 2: 计算m_z的分享
    bool m0[2]; // m的最高位
    for (int i = 0; i < 2; i++) {
        m0[i] = (input_origin[i].v1 >> (l_input - 1)) & 1;
    }

    // Step 2.1: [m_z] += [Gamma]
    mz_share.v += Gamma.v;

    // Step 2.2: [m_z] += m_x*m_y
    if (party_id == 1) {
        mz_share.v += input_origin[0].v1 * input_origin[1].v1;
    }

    // Step 2.3: [m_z] += [ m_x*(r_y_1+r_y_2) + m_y*(r_x_1+r_x_2) ]
    for (int i = 0; i < 2; i++) {
        mz_share.v += input_origin[i].v1 * input_origin[1 - i].v2;
    }

    // Step 2.4: [m_z] -= [1-r_a(0)] * m_b * m_a(0) *2^l
    for (int i = 0; i < 2; i++) {
        ShareValue temp =
            (-lf_share1[i].v + (party_id == 1 ? 1 : 0)) * (input_origin[1 - i].v1) * (m0[i]);
        mz_share.v -= (temp << l_input);
    }

    // Step 2.5: [m_z] -= [w_a(0)+r_a(0)] * m_b * 2^l
    for (int i = 0; i < 2; i++) {
        ShareValue temp = (lf_share2[i].v) * (input_origin[1 - i].v1);
        mz_share.v -= (temp << l_input);
    }

    // Step 2.6: [m_z] -= [(1 - r_a(0)) * (r_b_1 + r_b_2)] * m_a(0) * 2^l
    for (int i = 0; i < 2; i++) {
        ShareValue temp = (lf_share3[i].v) * (m0[i]);
        mz_share.v -= (temp << l_input);
    }

    // Step 2.7 - 2.9: 如果有offset，进行相应的修正
    if (has_offset) {
        // Step 2.7: [m_z] -= (m_x + m_y + [r_x] + [r_y]) * 2^(l-2)
        for (int i = 0; i < 2; i++) {
            ShareValue temp = input_origin[i].v2;
            if (party_id == 1) {
                temp += input_origin[i].v1;
            }
            mz_share.v -= (temp << (l_input - 2));
        }

        // Step 2.8: [m_z] += ( [w_a(0)+r_a(0)] - m_a(0)[r_a(0)] + m_a(0) ) * 2^(2l-2)
        for (int i = 0; i < 2; i++) {
            ShareValue temp = lf_share2[i].v - (m0[i] * lf_share1[i].v);
            if (party_id == 1) {
                temp += m0[i];
            }
            if (2 * l_input - 2 < ShareValue_BitLength) {
                mz_share.v += (temp << (2 * l_input - 2));
            }
        }

        // Step 2.9: [m_z] += 2^(2l-4)
        if (party_id == 1) {
            if (2 * l_input - 4 < ShareValue_BitLength) {
                mz_share.v += (ShareValue(1) << (2 * l_input - 4));
            }
        }
    }

    // Step 2.10: [m_z] =([m_z] >> lf) + 1 - [r_z]
    mz_share.v &= mz_share.MASK;
    mz_share.v >>= lf;
    if (party_id == 1) {
        mz_share.v += 1;
    }
    mz_share.v -= z.v2;

    // Step 2.11: 取模到l_res位
    ADDshare<> mz_share2(l_res);
#ifdef DEBUG_MODE
    mz_share2.has_shared = true;
#endif
    mz_share2.v = mz_share.v & mz_share2.MASK;

    // Step 3: 计算最终结果z
    z.v1 = ADDshare_recon(party_id, netio, &mz_share2);
}

void PI_fixed_mult_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                       std::vector<PI_fixed_mult_intermediate *> &PI_fixed_mult_intermediate_vec,
                       std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare *> &input_y_vec,
                       std::vector<MSSshare *> &output_z_vec, bool has_offset) {
#ifdef DEBUG_MODE
    if (PI_fixed_mult_intermediate_vec.size() != input_x_vec.size() ||
        input_x_vec.size() != input_y_vec.size() || input_y_vec.size() != output_z_vec.size()) {
        error("PI_fixed_mult_vec: input vector sizes do not match");
    }
    for (size_t i = 0; i < input_x_vec.size(); i++) {
        PI_fixed_mult_intermediate &PI_fixed_mult_intermediate = *PI_fixed_mult_intermediate_vec[i];
        MSSshare *input_x = input_x_vec[i];
        MSSshare *input_y = input_y_vec[i];
        MSSshare *output_z = output_z_vec[i];
        int li = PI_fixed_mult_intermediate.li;
        int lf = PI_fixed_mult_intermediate.lf;
        int l_res = PI_fixed_mult_intermediate.l_res;
        if (!input_x->has_shared) {
            error("PI_fixed_mult: input_x must be shared before calling PI_fixed_mult");
        }
        if (!input_y->has_shared) {
            error("PI_fixed_mult: input_y must be shared before calling PI_fixed_mult");
        }
        if (!PI_fixed_mult_intermediate.has_preprocess) {
            error("PI_fixed_mult: PI_fixed_mult_intermediate must be preprocessed before calling "
                  "PI_fixed_mult");
        }
        if (!output_z->has_preprocess) {
            error("PI_fixed_mult: output_z must be preprocessed before calling PI_fixed_mult");
        }
        if (input_x->BITLEN != li + lf || input_y->BITLEN != li + lf || output_z->BITLEN != l_res) {
            error("PI_fixed_mult_preprocess: input_x, input_y or output_z bitlength mismatch");
        }
        output_z->has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }

    int vec_size = input_x_vec.size();
    std::vector<ADDshare<>> mz_share2_vec;
    for (int i = 0; i < vec_size; i++) {
        PI_fixed_mult_intermediate &PI_fixed_mult_intermediate = *PI_fixed_mult_intermediate_vec[i];
        int li = PI_fixed_mult_intermediate.li;
        int lf = PI_fixed_mult_intermediate.lf;
        int l_res = PI_fixed_mult_intermediate.l_res;
        const int l_input = li + lf;
        MSSshare &x = *input_x_vec[i];
        MSSshare &y = *input_y_vec[i];
        MSSshare &z = *output_z_vec[i];
        ADDshare<> &Gamma = PI_fixed_mult_intermediate.Gamma;
        ADDshare<> *lf_share1 = PI_fixed_mult_intermediate.lf_share1;
        ADDshare<> *lf_share2 = PI_fixed_mult_intermediate.lf_share2;
        ADDshare<> *lf_share3 = PI_fixed_mult_intermediate.lf_share3;

        // Step 1: 如果有offset，将x和y的m加上2^(l-2)，之后视为lf + l_res位的分享
        MSSshare input_origin[2] = {x, y};
        for (int i = 0; i < 2; i++) {
            if (has_offset) {
                input_origin[i].v1 += (ShareValue(1) << (l_input - 2));
            }
            input_origin[i].v1 &= input_origin[i].MASK;
            input_origin[i].v2 &= input_origin[i].MASK;
        }

        ADDshare<> mz_share(lf + l_res); // 结果的m值的分享
#ifdef DEBUG_MODE
        mz_share.has_shared = true;
#endif

        // Step 2: 计算m_z的分享
        bool m0[2]; // m的最高位
        for (int i = 0; i < 2; i++) {
            m0[i] = (input_origin[i].v1 >> (l_input - 1)) & 1;
        }

        // Step 2.1: [m_z] += [Gamma]
        mz_share.v += Gamma.v;

        // Step 2.2: [m_z] += m_x*m_y
        if (party_id == 1) {
            mz_share.v += input_origin[0].v1 * input_origin[1].v1;
        }

        // Step 2.3: [m_z] += [ m_x*(r_y_1+r_y_2) + m_y*(r_x_1+r_x_2) ]
        for (int i = 0; i < 2; i++) {
            mz_share.v += input_origin[i].v1 * input_origin[1 - i].v2;
        }

        // Step 2.4: [m_z] -= [1-r_a(0)] * m_b * m_a(0) *2^l
        for (int i = 0; i < 2; i++) {
            ShareValue temp =
                (-lf_share1[i].v + (party_id == 1 ? 1 : 0)) * (input_origin[1 - i].v1) * (m0[i]);
            mz_share.v -= (temp << l_input);
        }

        // Step 2.5: [m_z] -= [w_a(0)+r_a(0)] * m_b * 2^l
        for (int i = 0; i < 2; i++) {
            ShareValue temp = (lf_share2[i].v) * (input_origin[1 - i].v1);
            mz_share.v -= (temp << l_input);
        }

        // Step 2.6: [m_z] -= [(1 - r_a(0)) * (r_b_1 + r_b_2)] * m_a(0) * 2^l
        for (int i = 0; i < 2; i++) {
            ShareValue temp = (lf_share3[i].v) * (m0[i]);
            mz_share.v -= (temp << l_input);
        }

        // Step 2.7 - 2.9: 如果有offset，进行相应的修正
        if (has_offset) {
            // Step 2.7: [m_z] -= (m_x + m_y + [r_x] + [r_y]) * 2^(l-2)
            for (int i = 0; i < 2; i++) {
                ShareValue temp = input_origin[i].v2;
                if (party_id == 1) {
                    temp += input_origin[i].v1;
                }
                mz_share.v -= (temp << (l_input - 2));
            }

            // Step 2.8: [m_z] += ( [w_a(0)+r_a(0)] - m_a(0)[r_a(0)] + m_a(0) ) * 2^(2l-2)
            for (int i = 0; i < 2; i++) {
                ShareValue temp = lf_share2[i].v - (m0[i] * lf_share1[i].v);
                if (party_id == 1) {
                    temp += m0[i];
                }
                if (2 * l_input - 2 < ShareValue_BitLength) {
                    mz_share.v += (temp << (2 * l_input - 2));
                }
            }

            // Step 2.9: [m_z] += 2^(2l-4)
            if (party_id == 1) {
                if (2 * l_input - 4 < ShareValue_BitLength) {
                    mz_share.v += (ShareValue(1) << (2 * l_input - 4));
                }
            }
        }

        // Step 2.10: [m_z] =([m_z] >> lf) + 1 - [r_z]
        mz_share.v &= mz_share.MASK;
        mz_share.v >>= lf;
        if (party_id == 1) {
            mz_share.v += 1;
        }
        mz_share.v -= z.v2;

        // Step 2.11: 取模到l_res位
        ADDshare<> mz_share2(l_res);
#ifdef DEBUG_MODE
        mz_share2.has_shared = true;
#endif
        mz_share2.v = mz_share.v & mz_share2.MASK;
        mz_share2_vec.push_back(mz_share2);
    }

    encoded_msg msg, msg_recv;
    int total_bitlen = 0;
    int total_bytelen = 0;
    for (int i = 0; i < vec_size; i++) {
        msg.add_msg((char *)&mz_share2_vec[i].v, mz_share2_vec[i].BITLEN);
        total_bitlen += mz_share2_vec[i].BITLEN;
    }
    total_bytelen = (total_bitlen + 7) / 8;
    std::vector<char> tmp(total_bytelen);
    if(party_id == 1) {
        msg.to_char_array(tmp.data());
        netio.send_data(2, tmp.data(), total_bytelen);

        netio.recv_data(2, tmp.data(), total_bytelen);
        msg_recv.from_char_array(tmp.data(), total_bytelen);
    } else if(party_id == 2) {
        netio.recv_data(1, tmp.data(), total_bytelen);
        msg_recv.from_char_array(tmp.data(), total_bytelen);

        msg.to_char_array(tmp.data());
        netio.send_data(1, tmp.data(), total_bytelen);
    }
    for (int i = 0; i < vec_size; i++) {
        MSSshare &z = *output_z_vec[i];
        ADDshare<> &mz_share2 = mz_share2_vec[i];
        ShareValue temp = 0;
        msg_recv.read_msg((char *)&temp, mz_share2.BITLEN);
        z.v1 = (temp+ mz_share2.v) & z.MASK;
    }
}