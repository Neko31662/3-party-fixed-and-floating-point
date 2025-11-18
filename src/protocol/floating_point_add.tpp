void PI_float_add_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                             PRGSync private_PRG, PI_float_add_intermediate &intermediate,
                             FLTshare *input_x, FLTshare *input_y, FLTshare *output_z) {
    int lf = intermediate.lf;
    int le = intermediate.le;
#ifdef DEBUG_MODE
    if (intermediate.has_preprocess) {
        error("PI_float_add_preprocess has already been called on this object");
    }
    if (!input_x->has_preprocess) {
        error("PI_float_add_preprocess: input_x must be preprocessed before calling "
              "PI_float_add_preprocess");
    }
    if (!input_y->has_preprocess) {
        error("PI_float_add_preprocess: input_y must be preprocessed before calling "
              "PI_float_add_preprocess");
    }
    if (output_z->has_preprocess) {
        error("PI_float_add_preprocess: output_z has already been preprocessed");
    }
    if (input_x->lf != lf || input_x->le != le) {
        error("PI_float_add_preprocess: input_x format mismatch");
    }
    if (input_y->lf != lf || input_y->le != le) {
        error("PI_float_add_preprocess: input_y format mismatch");
    }
    if (output_z->lf != lf || output_z->le != le) {
        error("PI_float_add_preprocess: output_z format mismatch");
    }
    intermediate.has_preprocess = true;
#endif

    FLTshare &x = *input_x;
    FLTshare &y = *input_y;
    FLTshare &z = *output_z;
    PI_sign_intermediate &sign_inter1 = intermediate.sign_inter1;
    PI_sign_intermediate &sign_inter2 = intermediate.sign_inter2;
    PI_great_intermediate &great_inter1 = intermediate.great_inter1;
    PI_eq_intermediate &eq_inter1 = intermediate.eq_inter1;
    PI_select_FLT_intermediate &select_FLT_inter1 = intermediate.select_FLT_inter1;
    PI_select_FLT_intermediate &select_FLT_inter2 = intermediate.select_FLT_inter2;
    PI_wrap2_spec_intermediate &wrap_inter1 = intermediate.wrap_inter1;
    PI_shift_intermediate &shift_inter1 = intermediate.shift_inter1;
    PI_select_intermediate &select_inter3 = intermediate.select_inter3;
    PI_select_intermediate &select_inter4 = intermediate.select_inter4;
    PI_align_intermediate &align_inter1 = intermediate.align_inter1;
    PI_trunc_intermediate &trunc_inter1 = intermediate.trunc_inter1;
    MSSshare_p &c1 = intermediate.c1;
    MSSshare_p &c2 = intermediate.c2;
    MSSshare_p &c3 = intermediate.c3;
    MSSshare_p &d = intermediate.d;
    MSSshare_mul_res &c_tmp1 = intermediate.c_tmp1;
    MSSshare_mul_res &c_tmp2 = intermediate.c_tmp2;
    MSSshare &c = intermediate.c;
    FLTshare &X = intermediate.X;
    FLTshare &Y = intermediate.Y;
    MSSshare_p &e = intermediate.e;
    MSSshare_mul_res &Y_t_prime1 = intermediate.Y_t_prime1;
    MSSshare &Y_t_prime2 = intermediate.Y_t_prime2;
    MSSshare &z_t_prime1 = intermediate.z_t_prime1;
    MSSshare &z_t_prime2 = intermediate.z_t_prime2;
    MSSshare_mul_res &zeta = intermediate.zeta;

    // Step 1:c1,c2,c3
    MSSshare dif_e = input_x->e - input_y->e;
    MSSshare dif_t = input_x->t - input_y->t;
    PI_sign_preprocess(party_id, PRGs, netio, sign_inter1, &dif_e, &c1);
    PI_eq_preprocess(party_id, PRGs, netio, private_PRG, eq_inter1, &input_x->e, &input_y->e, &c2);
    PI_sign_preprocess(party_id, PRGs, netio, sign_inter2, &input_x->t, &c3);
    MSSshare c1_mss(1), c2_mss(1), c3_mss(1);
    c1_mss = c1 * (-1);
    c2_mss = c2;
    c3_mss = c3 * (-1);

    // Step 2:c_tmp1,c_tmp2,c
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &c_tmp1, &c2_mss, &c3_mss);
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &c_tmp2, &c1_mss, &c2_mss);
    c = c_tmp1 + c1_mss - c_tmp2;

    // Step 3: 根据c选择X,Y
    auto select_inter_vec = make_ptr_vec(select_FLT_inter1.b_inter, select_FLT_inter1.t_inter,
                                         select_FLT_inter1.e_inter, select_FLT_inter2.b_inter,
                                         select_FLT_inter2.t_inter, select_FLT_inter2.e_inter);
    auto select_input_1_vec = make_ptr_vec(x.b, x.t, x.e, y.b, y.t, y.e);
    auto select_input_2_vec = make_ptr_vec(y.b, y.t, y.e, x.b, x.t, x.e);
    auto select_input_3_vec = make_ptr_vec(c, c, c, c, c, c);
    auto select_output_vec = make_ptr_vec(X.b, X.t, X.e, Y.b, Y.t, Y.e);
    for (int i = 0; i < 6; i++) {
        PI_select_preprocess(party_id, PRGs, netio, *select_inter_vec[i], select_input_1_vec[i],
                             select_input_2_vec[i], select_input_3_vec[i], select_output_vec[i]);
    }

    // Step 4: 计算 zeta1 (le bits) = X.e - Y.e
    MSSshare zeta1 = X.e - Y.e;

    // Step 5: 本地生成明文分享 lf_mss (le bits)，r1=r2=0，m=lf
    MSSshare lf_mss(le);
#ifdef DEBUG_MODE
    lf_mss.has_preprocess = true;
#endif
    if (party_id == 1 || party_id == 2) {
        lf_mss.v1 = lf & lf_mss.MASK;
    }

    // Step 6: 计算 d = PI_great(zeta1, lf_mss)，d为 1 bit
    PI_great_preprocess(party_id, PRGs, netio, great_inter1, &zeta1, &lf_mss, &d);
    MSSshare d_mss(1);
    d_mss = d;

    // Step 7: 计算 e = PI_wrap2_spec(Y.t)，e为 2 + 2 * lf bits
    PI_wrap2_spec_preprocess(party_id, PRGs, netio, wrap_inter1, &Y.t, &e);

    // Step 8: 计算 Y_t_prime3 = Y.t - 2^(lf + 2) * e (mod 2^(2+2*lf) )
    MSSshare Y_t_prime3(2 + 2 * lf);
#ifdef DEBUG_MODE
    Y_t_prime3.has_preprocess = true;
#endif
    Y_t_prime3.v1 = (Y.t.v1 - (e.v1 << (lf + 2))) & Y_t_prime3.MASK;
    Y_t_prime3.v2 = (Y.t.v2 - (e.v2 << (lf + 2))) & Y_t_prime3.MASK;

    // Step 9: 计算 Y_t_prime1 = PI_shift(Y_t_prime3, lf - zeta1)
    PI_shift_preprocess(party_id, PRGs, netio, shift_inter1, &Y_t_prime3, &Y_t_prime1);

    // Step 10: 将Y_t_prime1右移 lf bits，得到Y_t_prime4
    MSSshare Y_t_prime4(lf + 2);
#ifdef DEBUG_MODE
    Y_t_prime4.has_preprocess = true;
#endif
    Y_t_prime4.v1 = (Y_t_prime1.v1 >> lf) & Y_t_prime4.MASK;
    Y_t_prime4.v2 = (Y_t_prime1.v2 >> lf) & Y_t_prime4.MASK;

    // Step 11: 根据d选择 0 或者 Y_t_prime4 得到 Y_t_prime2 (2 + lf bits)
    MSSshare zero_mss(2 + lf);
#ifdef DEBUG_MODE
    zero_mss.has_preprocess = true;
#endif
    PI_select_preprocess(party_id, PRGs, netio, select_inter3, &zero_mss, &Y_t_prime4, &d_mss,
                         &Y_t_prime2);

    // Step 12: 计算 z1 = X.t + Y_t_prime2 , z2 = X.t - Y_t_prime2
    MSSshare z1(2 + lf), z2(2 + lf);
#ifdef DEBUG_MODE
    z1.has_preprocess = true;
    z2.has_preprocess = true;
#endif
    z1.v1 = (X.t.v1 + Y_t_prime2.v1) & z1.MASK;
    z1.v2 = (X.t.v2 + Y_t_prime2.v2) & z1.MASK;
    z2.v1 = (X.t.v1 - Y_t_prime2.v1) & z2.MASK;
    z2.v2 = (X.t.v2 - Y_t_prime2.v2) & z2.MASK;

    // Step 13: 计算 g = X.b + Y.b (mod 2)
    MSSshare g(1);
#ifdef DEBUG_MODE
    g.has_preprocess = true;
#endif
    g.v1 = (X.b.v1 + Y.b.v1) & g.MASK;
    g.v2 = (X.b.v2 + Y.b.v2) & g.MASK;

    // Step 14: z.b = X.b
    z.b = X.b;

    // Step 15: 根据g选择 z_t_prime1 = z2 或者 z1
    PI_select_preprocess(party_id, PRGs, netio, select_inter4, &z2, &z1, &g, &z_t_prime1);

    // Step 16: 计算 z_t_prime2, zeta = PI_align(z_t_prime1)
    PI_align_preprocess(party_id, PRGs, netio, align_inter1, &z_t_prime1, &z_t_prime2, &zeta);

    // Step 17: 计算 z.t = PI_trunc(z_t_prime2,1)
    PI_trunc_preprocess(party_id, PRGs, netio, trunc_inter1, &z_t_prime2, 1, &z.t);

    // Step 18: 计算 z.e = X.e - zeta + 1
    z.e = X.e - zeta;
}

void PI_float_add(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  PI_float_add_intermediate &intermediate, FLTshare *input_x, FLTshare *input_y,
                  FLTshare *output_z) {
    int lf = intermediate.lf;
    int le = intermediate.le;
#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_float_add: preprocess must be done before calling PI_float_add");
    }
    if (!input_x->has_shared) {
        error("PI_float_add: input_x must be shared before calling PI_float_add");
    }
    if (!input_y->has_shared) {
        error("PI_float_add: input_y must be shared before calling PI_float_add");
    }
    if (output_z->has_shared) {
        error("PI_float_add: output_z has already been shared");
    }
    if (input_x->lf != lf || input_x->le != le) {
        error("PI_float_add: input_x format mismatch");
    }
    if (input_y->lf != lf || input_y->le != le) {
        error("PI_float_add: input_y format mismatch");
    }
    if (output_z->lf != lf || output_z->le != le) {
        error("PI_float_add: output_z format mismatch");
    }
    output_z->has_shared = true;
#endif

    FLTshare &x = *input_x;
    FLTshare &y = *input_y;
    FLTshare &z = *output_z;
    PI_sign_intermediate &sign_inter1 = intermediate.sign_inter1;
    PI_sign_intermediate &sign_inter2 = intermediate.sign_inter2;
    PI_great_intermediate &great_inter1 = intermediate.great_inter1;
    PI_eq_intermediate &eq_inter1 = intermediate.eq_inter1;
    PI_select_FLT_intermediate &select_FLT_inter1 = intermediate.select_FLT_inter1;
    PI_select_FLT_intermediate &select_FLT_inter2 = intermediate.select_FLT_inter2;
    PI_wrap2_spec_intermediate &wrap_inter1 = intermediate.wrap_inter1;
    PI_shift_intermediate &shift_inter1 = intermediate.shift_inter1;
    PI_select_intermediate &select_inter3 = intermediate.select_inter3;
    PI_select_intermediate &select_inter4 = intermediate.select_inter4;
    PI_align_intermediate &align_inter1 = intermediate.align_inter1;
    PI_trunc_intermediate &trunc_inter1 = intermediate.trunc_inter1;
    MSSshare_p &c1 = intermediate.c1;
    MSSshare_p &c2 = intermediate.c2;
    MSSshare_p &c3 = intermediate.c3;
    MSSshare_p &d = intermediate.d;
    MSSshare_mul_res &c_tmp1 = intermediate.c_tmp1;
    MSSshare_mul_res &c_tmp2 = intermediate.c_tmp2;
    MSSshare &c = intermediate.c;
    FLTshare &X = intermediate.X;
    FLTshare &Y = intermediate.Y;
    MSSshare_p &e = intermediate.e;
    MSSshare_mul_res &Y_t_prime1 = intermediate.Y_t_prime1;
    MSSshare &Y_t_prime2 = intermediate.Y_t_prime2;
    MSSshare &z_t_prime1 = intermediate.z_t_prime1;
    MSSshare &z_t_prime2 = intermediate.z_t_prime2;
    MSSshare_mul_res &zeta = intermediate.zeta;

    // Step 1:c1,c2,c3
    auto dif_e = input_x->e - input_y->e;
    auto dif_t = input_x->t - input_y->t;
    auto sign_inter_vec = make_ptr_vec<PI_sign_intermediate>(sign_inter1, sign_inter2);
    auto sign_input_x_vec = make_ptr_vec<MSSshare>(dif_e, dif_t);
    auto sign_output_vec = make_ptr_vec<MSSshare_p>(c1, c3);

    PI_sign_vec(party_id, PRGs, netio, sign_inter_vec, sign_input_x_vec, sign_output_vec);
    PI_eq(party_id, PRGs, netio, eq_inter1, &input_x->e, &input_y->e, &c2);
    MSSshare c1_mss(1), c2_mss(1), c3_mss(1);
    c1_mss = c1 * (-1), MSSshare_add_plain(party_id, &c1_mss, 1);
    c2_mss = c2;
    c3_mss = c3 * (-1), MSSshare_add_plain(party_id, &c3_mss, 1);

    // Step 2:c_tmp1,c_tmp2,c
    auto mul_res_vec_1 = make_ptr_vec<MSSshare_mul_res>(c_tmp1, c_tmp2);
    auto mul_s1_vec_1 = make_ptr_vec<MSSshare>(c2_mss, c1_mss);
    auto mul_s2_vec_1 = make_ptr_vec<MSSshare>(c3_mss, c2_mss);
    MSSshare_mul_res_calc_mul_vec(party_id, netio, mul_res_vec_1, mul_s1_vec_1, mul_s2_vec_1);
    c = c_tmp1 + c1_mss - c_tmp2;

    // Step 3: 根据c选择X,Y
    auto select_inter_vec = make_ptr_vec(select_FLT_inter1.b_inter, select_FLT_inter1.t_inter,
                                         select_FLT_inter1.e_inter, select_FLT_inter2.b_inter,
                                         select_FLT_inter2.t_inter, select_FLT_inter2.e_inter);
    auto select_input_1_vec = make_ptr_vec(x.b, x.t, x.e, y.b, y.t, y.e);
    auto select_input_2_vec = make_ptr_vec(y.b, y.t, y.e, x.b, x.t, x.e);
    auto select_input_3_vec = make_ptr_vec(c, c, c, c, c, c);
    auto select_output_vec = make_ptr_vec(X.b, X.t, X.e, Y.b, Y.t, Y.e);
    PI_select_vec(party_id, PRGs, netio, select_inter_vec, select_input_1_vec, select_input_2_vec,
                  select_input_3_vec, select_output_vec);

    // Step 4: 计算 zeta1 (le bits) = X.e - Y.e
    MSSshare zeta1 = X.e - Y.e;

    // Step 5: 本地生成明文分享 lf_mss (le bits)，r1=r2=0，m=lf
    MSSshare lf_mss(le);
#ifdef DEBUG_MODE
    lf_mss.has_preprocess = true;
    lf_mss.has_shared = true;
#endif
    if (party_id == 1 || party_id == 2) {
        lf_mss.v1 = lf & lf_mss.MASK;
    }

    // Step 6: 计算 d = PI_great(zeta1, lf_mss)，d为 1 bit
    PI_great(party_id, PRGs, netio, great_inter1, &zeta1, &lf_mss, &d);

    // Step 7: 计算 e = PI_wrap2_spec(Y.t)，e为 2 + 2 * lf bits
    PI_wrap2_spec(party_id, PRGs, netio, wrap_inter1, &Y.t, &e);

    // Step 8: 计算 Y_t_prime3 = Y.t - 2^(lf + 2) * e (mod 2^(2+2*lf) )
    MSSshare Y_t_prime3(2 + 2 * lf);
#ifdef DEBUG_MODE
    Y_t_prime3.has_preprocess = true;
    Y_t_prime3.has_shared = true;
#endif
    Y_t_prime3.v1 = (Y.t.v1 - (e.v1 << (lf + 2))) & Y_t_prime3.MASK;
    Y_t_prime3.v2 = (Y.t.v2 - (e.v2 << (lf + 2))) & Y_t_prime3.MASK;

    // Step 9: 计算 Y_t_prime1 = PI_shift(Y_t_prime3, lf - zeta1)
    ADDshare<> shift_input_k(LOG_1(shift_inter1.ell));
#ifdef DEBUG_MODE
    shift_input_k.has_shared = true;
#endif
    if (party_id == 1) {
        shift_input_k.v = lf - zeta1.v1 - zeta1.v2;
    } else if (party_id == 2) {
        shift_input_k.v = -zeta1.v2;
    }
    shift_input_k.v &= shift_input_k.MASK;
    PI_shift(party_id, PRGs, netio, shift_inter1, &Y_t_prime3, &shift_input_k, &Y_t_prime1);

    // Step 10: 将Y_t_prime1右移 lf bits，得到Y_t_prime4
    MSSshare Y_t_prime4(lf + 2);
#ifdef DEBUG_MODE
    Y_t_prime4.has_preprocess = true;
    Y_t_prime4.has_shared = true;
#endif
    Y_t_prime4.v1 = (Y_t_prime1.v1 >> lf) & Y_t_prime4.MASK;
    Y_t_prime4.v2 = (Y_t_prime1.v2 >> lf) & Y_t_prime4.MASK;

    // Step 11: 根据d选择 0 或者 Y_t_prime4 得到 Y_t_prime2 (2 + lf bits)
    MSSshare d_mss(1);
    d_mss = d;
    MSSshare zero_mss(2 + lf);
#ifdef DEBUG_MODE
    zero_mss.has_preprocess = true;
    zero_mss.has_shared = true;
#endif
    PI_select(party_id, PRGs, netio, select_inter3, &zero_mss, &Y_t_prime4, &d_mss, &Y_t_prime2);

    // Step 12: 计算 z1 = X.t + Y_t_prime2 , z2 = X.t - Y_t_prime2
    MSSshare z1(2 + lf), z2(2 + lf);
#ifdef DEBUG_MODE
    z1.has_preprocess = true;
    z1.has_shared = true;
    z2.has_preprocess = true;
    z2.has_shared = true;
#endif
    z1.v1 = (X.t.v1 + Y_t_prime2.v1) & z1.MASK;
    z1.v2 = (X.t.v2 + Y_t_prime2.v2) & z1.MASK;
    z2.v1 = (X.t.v1 - Y_t_prime2.v1) & z2.MASK;
    z2.v2 = (X.t.v2 - Y_t_prime2.v2) & z2.MASK;

    // Step 13: 计算 g = X.b + Y.b (mod 2)
    MSSshare g(1);
#ifdef DEBUG_MODE
    g.has_preprocess = true;
    g.has_shared = true;
#endif
    g.v1 = (X.b.v1 + Y.b.v1) & g.MASK;
    g.v2 = (X.b.v2 + Y.b.v2) & g.MASK;

    // Step 14: z.b = X.b
    z.b = X.b;

    // Step 15: 根据g选择 z_t_prime1 = z2 或者 z1
    PI_select(party_id, PRGs, netio, select_inter4, &z2, &z1, &g, &z_t_prime1);

    // Step 16: 计算 z_t_prime2, zeta = PI_align(z_t_prime1)
    PI_align(party_id, PRGs, netio, align_inter1, &z_t_prime1, &z_t_prime2, &zeta);

    // Step 17: 计算 z.t = PI_trunc(z_t_prime2,1)
    PI_trunc(party_id, PRGs, netio, trunc_inter1, &z_t_prime2, 1, &z.t);

    // Step 18: 计算 z.e = X.e - zeta + 1
    z.e = X.e - zeta;
    MSSshare_add_plain(party_id, &z.e, 1);

    // for debug
    // ShareValue c1_plain, c2_plain, c3_plain, c_plain;
    // c1_plain = MSSshare_recon(party_id, netio, &c1_mss);
    // c2_plain = MSSshare_recon(party_id, netio, &c2_mss);
    // c3_plain = MSSshare_recon(party_id, netio, &c3_mss);
    // c_plain = MSSshare_recon(party_id, netio, &c);
    // std::cout << "c1=" << c1_plain << ", c2=" << c2_plain << ", c3=" << c3_plain
    //           << ", c=" << c_plain << std::endl;

    // ShareValue X_b_plain, X_t_plain, X_e_plain;
    // X_b_plain = MSSshare_recon(party_id, netio, &X.b);
    // X_t_plain = MSSshare_recon(party_id, netio, &X.t);
    // X_e_plain = MSSshare_recon(party_id, netio, &X.e);
    // std::cout << "X.b= " << std::bitset<1>(X_b_plain) << std::endl;
    // std::cout << "X.t= " << std::bitset<32>(X_t_plain) << std::endl;
    // std::cout << "X.e= " << std::bitset<32>(X_e_plain) << std::endl;

    // ShareValue Y_b_plain, Y_t_plain, Y_e_plain;
    // Y_b_plain = MSSshare_recon(party_id, netio, &Y.b);
    // Y_t_plain = MSSshare_recon(party_id, netio, &Y.t);
    // Y_e_plain = MSSshare_recon(party_id, netio, &Y.e);
    // std::cout << "Y.b= " << std::bitset<1>(Y_b_plain) << std::endl;
    // std::cout << "Y.t= " << std::bitset<32>(Y_t_plain) << std::endl;
    // std::cout << "Y.e= " << std::bitset<32>(Y_e_plain) << std::endl;
}

void PI_float_add_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                      std::vector<PI_float_add_intermediate *> &intermediate_vec,
                      std::vector<FLTshare *> &input_x_vec, std::vector<FLTshare *> &input_y_vec,
                      std::vector<FLTshare *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != input_y_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_float_add_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); ++i) {
        PI_float_add_intermediate &intermediate = *intermediate_vec[i];
        FLTshare *input_x = input_x_vec[i];
        FLTshare *input_y = input_y_vec[i];
        FLTshare *output_z = output_z_vec[i];
        int lf = intermediate.lf;
        int le = intermediate.le;
        if (!intermediate.has_preprocess) {
            error("PI_float_add_vec: preprocess must be done before calling PI_float_add_vec");
        }
        if (!input_x->has_shared) {
            error("PI_float_add_vec: input_x must be shared before calling PI_float_add_vec");
        }
        if (!input_y->has_shared) {
            error("PI_float_add_vec: input_y must be shared before calling PI_float_add_vec");
        }
        if (output_z->has_shared) {
            error("PI_float_add_vec: output_z has already been shared");
        }
        if (input_x->lf != lf || input_x->le != le) {
            error("PI_float_add_vec: input_x format mismatch");
        }
        if (input_y->lf != lf || input_y->le != le) {
            error("PI_float_add_vec: input_y format mismatch");
        }
        if (output_z->lf != lf || output_z->le != le) {
            error("PI_float_add_vec: output_z format mismatch");
        }
        output_z->has_shared = true;
    }
#endif

#define USE_ALL_ELEMENTS                                                                           \
    PI_float_add_intermediate &intermediate = *intermediate_vec[idx];                              \
    FLTshare *input_x = input_x_vec[idx];                                                          \
    FLTshare *input_y = input_y_vec[idx];                                                          \
    FLTshare *output_z = output_z_vec[idx];                                                        \
    FLTshare &x = *input_x;                                                                        \
    FLTshare &y = *input_y;                                                                        \
    FLTshare &z = *output_z;                                                                       \
    PI_sign_intermediate &sign_inter1 = intermediate.sign_inter1;                                  \
    PI_sign_intermediate &sign_inter2 = intermediate.sign_inter2;                                  \
    PI_great_intermediate &great_inter1 = intermediate.great_inter1;                               \
    PI_eq_intermediate &eq_inter1 = intermediate.eq_inter1;                                        \
    PI_select_FLT_intermediate &select_FLT_inter1 = intermediate.select_FLT_inter1;                \
    PI_select_FLT_intermediate &select_FLT_inter2 = intermediate.select_FLT_inter2;                \
    PI_wrap2_spec_intermediate &wrap_inter1 = intermediate.wrap_inter1;                            \
    PI_shift_intermediate &shift_inter1 = intermediate.shift_inter1;                               \
    PI_select_intermediate &select_inter3 = intermediate.select_inter3;                            \
    PI_select_intermediate &select_inter4 = intermediate.select_inter4;                            \
    PI_align_intermediate &align_inter1 = intermediate.align_inter1;                               \
    PI_trunc_intermediate &trunc_inter1 = intermediate.trunc_inter1;                               \
    MSSshare_p &c1 = intermediate.c1;                                                              \
    MSSshare_p &c2 = intermediate.c2;                                                              \
    MSSshare_p &c3 = intermediate.c3;                                                              \
    MSSshare_p &d = intermediate.d;                                                                \
    MSSshare_mul_res &c_tmp1 = intermediate.c_tmp1;                                                \
    MSSshare_mul_res &c_tmp2 = intermediate.c_tmp2;                                                \
    MSSshare &c = intermediate.c;                                                                  \
    FLTshare &X = intermediate.X;                                                                  \
    FLTshare &Y = intermediate.Y;                                                                  \
    MSSshare_p &e = intermediate.e;                                                                \
    MSSshare_mul_res &Y_t_prime1 = intermediate.Y_t_prime1;                                        \
    MSSshare &Y_t_prime2 = intermediate.Y_t_prime2;                                                \
    MSSshare &z_t_prime1 = intermediate.z_t_prime1;                                                \
    MSSshare &z_t_prime2 = intermediate.z_t_prime2;                                                \
    MSSshare_mul_res &zeta = intermediate.zeta;                                                    \
    int &lf = intermediate.lf;                                                                     \
    int le = intermediate.le

    int vec_size = intermediate_vec.size();

    // Step 1:c1,c2,c3
    std::vector<PI_sign_intermediate *> sign_inter_ptr;
    std::vector<MSSshare> sign_input_x_vec;
    std::vector<MSSshare_p *> sign_output_ptr;
    sign_inter_ptr.reserve(vec_size * 2);
    sign_input_x_vec.reserve(vec_size * 2);
    sign_output_ptr.reserve(vec_size * 2);
    std::vector<PI_eq_intermediate *> eq_inter_ptr;
    std::vector<MSSshare *> eq_input_x_ptr;
    std::vector<MSSshare *> eq_input_y_ptr;
    std::vector<MSSshare_p *> eq_output_ptr;
    eq_inter_ptr.reserve(vec_size);
    eq_input_x_ptr.reserve(vec_size);
    eq_input_y_ptr.reserve(vec_size);
    eq_output_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        auto dif_e = input_x->e - input_y->e;
        auto dif_t = input_x->t - input_y->t;

        sign_inter_ptr.push_back(&sign_inter1);
        sign_inter_ptr.push_back(&sign_inter2);
        sign_input_x_vec.push_back(dif_e);
        sign_input_x_vec.push_back(dif_t);
        sign_output_ptr.push_back(&c1);
        sign_output_ptr.push_back(&c3);

        eq_inter_ptr.push_back(&eq_inter1);
        eq_input_x_ptr.push_back(&input_x->e);
        eq_input_y_ptr.push_back(&input_y->e);
        eq_output_ptr.push_back(&c2);
    }
    auto sign_input_x_ptr = make_ptr_vec<MSSshare>(sign_input_x_vec);
    PI_sign_vec(party_id, PRGs, netio, sign_inter_ptr, sign_input_x_ptr, sign_output_ptr);
    PI_eq_vec(party_id, PRGs, netio, eq_inter_ptr, eq_input_x_ptr, eq_input_y_ptr, eq_output_ptr);

    // Step 2:c_tmp1,c_tmp2,c
    std::vector<MSSshare_mul_res *> mul_res_ptr_1;
    std::vector<MSSshare> mul_s1_vec_1;
    std::vector<MSSshare> mul_s2_vec_1;
    mul_res_ptr_1.reserve(vec_size * 2);
    mul_s1_vec_1.reserve(vec_size * 2);
    mul_s2_vec_1.reserve(vec_size * 2);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        MSSshare c1_mss(1), c2_mss(1), c3_mss(1);
        c1_mss = c1 * (-1), MSSshare_add_plain(party_id, &c1_mss, 1);
        c2_mss = c2;
        c3_mss = c3 * (-1), MSSshare_add_plain(party_id, &c3_mss, 1);

        mul_res_ptr_1.push_back(&c_tmp1);
        mul_res_ptr_1.push_back(&c_tmp2);
        mul_s1_vec_1.push_back(c2_mss);
        mul_s1_vec_1.push_back(c1_mss);
        mul_s2_vec_1.push_back(c3_mss);
        mul_s2_vec_1.push_back(c2_mss);
    }
    auto mul_s1_ptr_1 = make_ptr_vec<MSSshare>(mul_s1_vec_1);
    auto mul_s2_ptr_1 = make_ptr_vec<MSSshare>(mul_s2_vec_1);
    MSSshare_mul_res_calc_mul_vec(party_id, netio, mul_res_ptr_1, mul_s1_ptr_1, mul_s2_ptr_1);

    // Step 3: 根据c选择X,Y
    std::vector<PI_select_intermediate *> select_inter_ptr;
    std::vector<MSSshare *> select_input_1_ptr;
    std::vector<MSSshare *> select_input_2_ptr;
    std::vector<MSSshare *> select_input_3_ptr;
    std::vector<MSSshare *> select_output_ptr;
    select_inter_ptr.reserve(vec_size * 6);
    select_input_1_ptr.reserve(vec_size * 6);
    select_input_2_ptr.reserve(vec_size * 6);
    select_input_3_ptr.reserve(vec_size * 6);
    select_output_ptr.reserve(vec_size * 6);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        c = c_tmp1 + mul_s1_vec_1[idx * 2 + 1] - c_tmp2;

        select_inter_ptr.push_back(&select_FLT_inter1.b_inter);
        select_inter_ptr.push_back(&select_FLT_inter1.t_inter);
        select_inter_ptr.push_back(&select_FLT_inter1.e_inter);
        select_inter_ptr.push_back(&select_FLT_inter2.b_inter);
        select_inter_ptr.push_back(&select_FLT_inter2.t_inter);
        select_inter_ptr.push_back(&select_FLT_inter2.e_inter);
        select_input_1_ptr.push_back(&x.b);
        select_input_1_ptr.push_back(&x.t);
        select_input_1_ptr.push_back(&x.e);
        select_input_1_ptr.push_back(&y.b);
        select_input_1_ptr.push_back(&y.t);
        select_input_1_ptr.push_back(&y.e);
        select_input_2_ptr.push_back(&y.b);
        select_input_2_ptr.push_back(&y.t);
        select_input_2_ptr.push_back(&y.e);
        select_input_2_ptr.push_back(&x.b);
        select_input_2_ptr.push_back(&x.t);
        select_input_2_ptr.push_back(&x.e);
        for (int tmp = 0; tmp < 6; tmp++) {
            select_input_3_ptr.push_back(&c);
        }
        select_output_ptr.push_back(&X.b);
        select_output_ptr.push_back(&X.t);
        select_output_ptr.push_back(&X.e);
        select_output_ptr.push_back(&Y.b);
        select_output_ptr.push_back(&Y.t);
        select_output_ptr.push_back(&Y.e);
    }
    PI_select_vec(party_id, PRGs, netio, select_inter_ptr, select_input_1_ptr, select_input_2_ptr,
                  select_input_3_ptr, select_output_ptr);

    std::vector<PI_great_intermediate *> great_inter1_ptr;
    std::vector<MSSshare> zeta1_vec;
    std::vector<MSSshare> lf_mss_vec;
    std::vector<MSSshare_p *> d_ptr;
    great_inter1_ptr.reserve(vec_size);
    zeta1_vec.reserve(vec_size);
    lf_mss_vec.reserve(vec_size);
    d_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        // Step 4: 计算 zeta1 (le bits) = X.e - Y.e
        MSSshare zeta1 = X.e - Y.e;

        // Step 5: 本地生成明文分享 lf_mss (le bits)，r1=r2=0，m=lf
        MSSshare lf_mss(le);
#ifdef DEBUG_MODE
        lf_mss.has_preprocess = true;
        lf_mss.has_shared = true;
#endif
        if (party_id == 1 || party_id == 2) {
            lf_mss.v1 = lf & lf_mss.MASK;
        }

        great_inter1_ptr.push_back(&great_inter1);
        zeta1_vec.push_back(zeta1);
        lf_mss_vec.push_back(lf_mss);
        d_ptr.push_back(&d);
    }
    // Step 6: 计算 d = PI_great(zeta1, lf_mss)，d为 1 bit
    auto zeta1_ptr = make_ptr_vec<MSSshare>(zeta1_vec);
    auto lf_mss_ptr = make_ptr_vec<MSSshare>(lf_mss_vec);
    PI_great_vec(party_id, PRGs, netio, great_inter1_ptr, zeta1_ptr, lf_mss_ptr, d_ptr);

    // Step 7: 计算 e = PI_wrap2_spec(Y.t)，e为 2 + 2 * lf bits
    std::vector<PI_wrap2_spec_intermediate *> wrap_inter1_ptr;
    std::vector<MSSshare *> Y_t_ptr;
    std::vector<MSSshare_p *> e_ptr;
    wrap_inter1_ptr.reserve(vec_size);
    Y_t_ptr.reserve(vec_size);
    e_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        wrap_inter1_ptr.push_back(&wrap_inter1);
        Y_t_ptr.push_back(&Y.t);
        e_ptr.push_back(&e);
    }
    PI_wrap2_spec_vec(party_id, PRGs, netio, wrap_inter1_ptr, Y_t_ptr, e_ptr);

    std::vector<PI_shift_intermediate *> shift_inter1_ptr;
    std::vector<MSSshare> Y_t_prime3_vec;
    std::vector<ADDshare<>> shift_input_k_vec;
    std::vector<MSSshare_mul_res *> Y_t_prime1_ptr;
    shift_inter1_ptr.reserve(vec_size);
    Y_t_prime3_vec.reserve(vec_size);
    shift_input_k_vec.reserve(vec_size);
    Y_t_prime1_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;

        // Step 8: 计算 Y_t_prime3 = Y.t - 2^(lf + 2) * e (mod 2^(2+2*lf) )
        MSSshare Y_t_prime3(2 + 2 * lf);
#ifdef DEBUG_MODE
        Y_t_prime3.has_preprocess = true;
        Y_t_prime3.has_shared = true;
#endif
        Y_t_prime3.v1 = (Y.t.v1 - (e.v1 << (lf + 2))) & Y_t_prime3.MASK;
        Y_t_prime3.v2 = (Y.t.v2 - (e.v2 << (lf + 2))) & Y_t_prime3.MASK;

        // Step 9: 计算 Y_t_prime1 = PI_shift(Y_t_prime3, lf - zeta1)
        ADDshare<> shift_input_k(LOG_1(shift_inter1.ell));
#ifdef DEBUG_MODE
        shift_input_k.has_shared = true;
#endif
        if (party_id == 1) {
            shift_input_k.v = lf - zeta1_vec[idx].v1 - zeta1_vec[idx].v2;
        } else if (party_id == 2) {
            shift_input_k.v = -zeta1_vec[idx].v2;
        }
        shift_input_k.v &= shift_input_k.MASK;

        shift_inter1_ptr.push_back(&shift_inter1);
        Y_t_prime3_vec.push_back(Y_t_prime3);
        shift_input_k_vec.push_back(shift_input_k);
        Y_t_prime1_ptr.push_back(&Y_t_prime1);
    }
    auto Y_t_prime3_ptr = make_ptr_vec<MSSshare>(Y_t_prime3_vec);
    auto shift_input_k_ptr = make_ptr_vec<ADDshare<>>(shift_input_k_vec);
    PI_shift_vec(party_id, PRGs, netio, shift_inter1_ptr, Y_t_prime3_ptr, shift_input_k_ptr,
                 Y_t_prime1_ptr);

    std::vector<PI_select_intermediate *> select_inter3_ptr;
    std::vector<MSSshare> zero_mss_vec;
    std::vector<MSSshare> Y_t_prime4_vec;
    std::vector<MSSshare> d_mss_vec;
    std::vector<MSSshare *> Y_t_prime2_ptr;
    select_inter3_ptr.reserve(vec_size);
    zero_mss_vec.reserve(vec_size);
    Y_t_prime4_vec.reserve(vec_size);
    d_mss_vec.reserve(vec_size);
    Y_t_prime2_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        // Step 10: 将Y_t_prime1右移 lf bits，得到Y_t_prime4
        MSSshare Y_t_prime4(lf + 2);
#ifdef DEBUG_MODE
        Y_t_prime4.has_preprocess = true;
        Y_t_prime4.has_shared = true;
#endif
        Y_t_prime4.v1 = (Y_t_prime1.v1 >> lf) & Y_t_prime4.MASK;
        Y_t_prime4.v2 = (Y_t_prime1.v2 >> lf) & Y_t_prime4.MASK;

        // Step 11: 根据d选择 0 或者 Y_t_prime4 得到 Y_t_prime2 (2 + lf bits)
        MSSshare d_mss(1);
        d_mss = d;
        MSSshare zero_mss(2 + lf);
#ifdef DEBUG_MODE
        zero_mss.has_preprocess = true;
        zero_mss.has_shared = true;
#endif

        select_inter3_ptr.push_back(&select_inter3);
        zero_mss_vec.push_back(zero_mss);
        Y_t_prime4_vec.push_back(Y_t_prime4);
        d_mss_vec.push_back(d_mss);
        Y_t_prime2_ptr.push_back(&Y_t_prime2);
    }
    auto zero_mss_ptr = make_ptr_vec<MSSshare>(zero_mss_vec);
    auto Y_t_prime4_ptr = make_ptr_vec<MSSshare>(Y_t_prime4_vec);
    auto d_mss_ptr = make_ptr_vec<MSSshare>(d_mss_vec);
    PI_select_vec(party_id, PRGs, netio, select_inter3_ptr, zero_mss_ptr, Y_t_prime4_ptr, d_mss_ptr,
                  Y_t_prime2_ptr);

    std::vector<PI_select_intermediate *> select_inter4_ptr;
    std::vector<MSSshare> z1_vec;
    std::vector<MSSshare> z2_vec;
    std::vector<MSSshare> g_vec;
    std::vector<MSSshare *> z_t_prime1_ptr;
    select_inter4_ptr.reserve(vec_size);
    z1_vec.reserve(vec_size);
    z2_vec.reserve(vec_size);
    g_vec.reserve(vec_size);
    z_t_prime1_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;

        // Step 12: 计算 z1 = X.t + Y_t_prime2 , z2 = X.t - Y_t_prime2
        MSSshare z1(2 + lf), z2(2 + lf);
#ifdef DEBUG_MODE
        z1.has_preprocess = true;
        z1.has_shared = true;
        z2.has_preprocess = true;
        z2.has_shared = true;
#endif
        z1.v1 = (X.t.v1 + Y_t_prime2.v1) & z1.MASK;
        z1.v2 = (X.t.v2 + Y_t_prime2.v2) & z1.MASK;
        z2.v1 = (X.t.v1 - Y_t_prime2.v1) & z2.MASK;
        z2.v2 = (X.t.v2 - Y_t_prime2.v2) & z2.MASK;

        // Step 13: 计算 g = X.b + Y.b (mod 2)
        MSSshare g(1);
#ifdef DEBUG_MODE
        g.has_preprocess = true;
        g.has_shared = true;
#endif
        g.v1 = (X.b.v1 + Y.b.v1) & g.MASK;
        g.v2 = (X.b.v2 + Y.b.v2) & g.MASK;

        // Step 14: z.b = X.b
        z.b = X.b;

        select_inter4_ptr.push_back(&select_inter4);
        z1_vec.push_back(z1);
        z2_vec.push_back(z2);
        g_vec.push_back(g);
        z_t_prime1_ptr.push_back(&z_t_prime1);
    }
    // Step 15: 根据g选择 z_t_prime1 = z2 或者 z1
    auto z1_ptr = make_ptr_vec<MSSshare>(z1_vec);
    auto z2_ptr = make_ptr_vec<MSSshare>(z2_vec);
    auto g_ptr = make_ptr_vec<MSSshare>(g_vec);
    PI_select_vec(party_id, PRGs, netio, select_inter4_ptr, z2_ptr, z1_ptr, g_ptr, z_t_prime1_ptr);

    std::vector<PI_align_intermediate *> align_inter1_ptr;
    std::vector<MSSshare *> z_t_prime2_ptr;
    std::vector<MSSshare_mul_res *> zeta_ptr;
    align_inter1_ptr.reserve(vec_size);
    z_t_prime2_ptr.reserve(vec_size);
    zeta_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        align_inter1_ptr.push_back(&align_inter1);
        z_t_prime2_ptr.push_back(&z_t_prime2);
        zeta_ptr.push_back(&zeta);
    }
    // Step 16: 计算 z_t_prime2, zeta = PI_align(z_t_prime1)
    PI_align_vec(party_id, PRGs, netio, align_inter1_ptr, z_t_prime1_ptr, z_t_prime2_ptr, zeta_ptr);

    std::vector<PI_trunc_intermediate *> trunc_inter1_ptr;
    std::vector<MSSshare *> z_t_ptr;
    trunc_inter1_ptr.reserve(vec_size);
    z_t_ptr.reserve(vec_size);
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        trunc_inter1_ptr.push_back(&trunc_inter1);
        z_t_ptr.push_back(&z.t);
    }
    // Step 17: 计算 z.t = PI_trunc(z_t_prime2,1)
    PI_trunc_vec(party_id, PRGs, netio, trunc_inter1_ptr, z_t_prime2_ptr, 1, z_t_ptr);

    // Step 18: 计算 z.e = X.e - zeta + 1
    for (size_t idx = 0; idx < vec_size; ++idx) {
        USE_ALL_ELEMENTS;
        z.e = X.e - zeta;
        MSSshare_add_plain(party_id, &z.e, 1);
    }

#undef USE_ALL_ELEMENTS
}