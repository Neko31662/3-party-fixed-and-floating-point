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
    PI_great_intermediate &great_inter1 = intermediate.great_inter1;
    PI_great_intermediate &great_inter2 = intermediate.great_inter2;
    PI_great_intermediate &great_inter3 = intermediate.great_inter3;
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
    PI_great_preprocess(party_id, PRGs, netio, great_inter1, &input_x->e, &input_y->e, &c1);
    PI_eq_preprocess(party_id, PRGs, netio, private_PRG, eq_inter1, &input_x->e, &input_y->e, &c2);
    PI_great_preprocess(party_id, PRGs, netio, great_inter2, &input_x->t, &input_y->t, &c3);
    MSSshare c1_mss(1), c2_mss(1), c3_mss(1);
    MSSshare_from_p(&c1_mss, &c1);
    MSSshare_from_p(&c2_mss, &c2);
    MSSshare_from_p(&c3_mss, &c3);

    // Step 2:c_tmp1,c_tmp2,c
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &c_tmp1, &c2_mss, &c3_mss);
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &c_tmp2, &c1_mss, &c2_mss);
    auto s1_vec = make_ptr_vec<MSSshare>(c_tmp1, c1_mss, c_tmp2);
    auto coeff1_vec = std::vector<int>{1, 1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &c, s1_vec, coeff1_vec);

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
    MSSshare zeta1(le);
    auto s2_vec = make_ptr_vec(X.e, Y.e);
    auto coeff2_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &zeta1, s2_vec, coeff2_vec);

    // Step 5: 本地生成明文分享 lf_mss (le bits)，r1=r2=0，m=lf
    MSSshare lf_mss(le);
#ifdef DEBUG_MODE
    lf_mss.has_preprocess = true;
#endif
    if (party_id == 1 || party_id == 2) {
        lf_mss.v1 = lf & lf_mss.MASK;
    }

    // Step 6: 计算 d = PI_great(zeta1, lf_mss)，d为 1 bit
    PI_great_preprocess(party_id, PRGs, netio, great_inter3, &zeta1, &lf_mss, &d);
    MSSshare d_mss(1);
    MSSshare_from_p(&d_mss, &d);

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
    auto s3_vec = make_ptr_vec<MSSshare>(X.e, zeta);
    auto coeff3_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &z.e, s3_vec, coeff3_vec);
}

void PI_float_add(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     PI_float_add_intermediate &intermediate, FLTshare *input_x,
                     FLTshare *input_y, FLTshare *output_z) {
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
    PI_great_intermediate &great_inter1 = intermediate.great_inter1;
    PI_great_intermediate &great_inter2 = intermediate.great_inter2;
    PI_great_intermediate &great_inter3 = intermediate.great_inter3;
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
    auto great_inter_vec = make_ptr_vec<PI_great_intermediate>(great_inter1, great_inter2);
    auto great_input_x_vec = make_ptr_vec<MSSshare>(input_x->e, input_x->t);
    auto great_input_y_vec = make_ptr_vec<MSSshare>(input_y->e, input_y->t);
    auto great_output_vec = make_ptr_vec<MSSshare_p>(c1, c3);

    PI_great_vec(party_id, PRGs, netio, great_inter_vec, great_input_x_vec, great_input_y_vec,
                 great_output_vec);
    PI_eq(party_id, PRGs, netio, eq_inter1, &input_x->e, &input_y->e, &c2);
    MSSshare c1_mss(1), c2_mss(1), c3_mss(1);
    MSSshare_from_p(&c1_mss, &c1);
    MSSshare_from_p(&c2_mss, &c2);
    MSSshare_from_p(&c3_mss, &c3);

    // Step 2:c_tmp1,c_tmp2,c
    auto mul_res_vec_1 = make_ptr_vec<MSSshare_mul_res>(c_tmp1, c_tmp2);
    auto mul_s1_vec_1 = make_ptr_vec<MSSshare>(c2_mss, c1_mss);
    auto mul_s2_vec_1 = make_ptr_vec<MSSshare>(c3_mss, c2_mss);
    MSSshare_mul_res_calc_mul_vec(party_id, netio, mul_res_vec_1, mul_s1_vec_1, mul_s2_vec_1);
    auto s1_vec = make_ptr_vec<MSSshare>(c_tmp1, c1_mss, c_tmp2);
    auto coeff1_vec = std::vector<int>{1, 1, -1};
    MSSshare_add_res_calc_add_multi(party_id, &c, s1_vec, coeff1_vec);

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
    MSSshare zeta1(le);
    auto s2_vec = make_ptr_vec(X.e, Y.e);
    auto coeff2_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &zeta1, s2_vec, coeff2_vec);
    MSSshare_add_res_calc_add_multi(party_id, &zeta1, s2_vec, coeff2_vec);

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
    PI_great(party_id, PRGs, netio, great_inter3, &zeta1, &lf_mss, &d);
    MSSshare d_mss(1);
    MSSshare_from_p(&d_mss, &d);

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
    auto s3_vec = make_ptr_vec<MSSshare>(X.e, zeta);
    auto coeff3_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &z.e, s3_vec, coeff3_vec);
    if (party_id == 1 || party_id == 2) {
        z.e.v1 = (z.e.v1 + 1) & z.e.MASK;
    }
}