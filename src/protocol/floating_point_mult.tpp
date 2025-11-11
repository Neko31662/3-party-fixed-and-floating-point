template <int lf, int le>
void PI_float_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_float_mult_intermediate<lf, le> &intermediate,
                              FLTshare<lf, le> *input_x, FLTshare<lf, le> *input_y,
                              FLTshare<lf, le> *output_z) {
#ifdef DEBUG_MODE
    if (intermediate.has_preprocess) {
        error("PI_float_mult_preprocess has already been called on this object");
    }
    if (!input_x->has_preprocess) {
        error("PI_float_mult_preprocess: input_x must be preprocessed before calling "
              "PI_float_mult_preprocess");
    }
    if (!input_y->has_preprocess) {
        error("PI_float_mult_preprocess: input_y must be preprocessed before calling "
              "PI_float_mult_preprocess");
    }
    if (output_z->has_preprocess) {
        error("PI_float_mult_preprocess: output_z has already been preprocessed");
    }
    intermediate.has_preprocess = true;
    output_z->has_preprocess = true;
#endif

    FLTshare<lf, le> &x = *input_x;
    FLTshare<lf, le> &y = *input_y;
    FLTshare<lf, le> &z = *output_z;
    MSSshare<lf + 3> &zhat1_lf_3 = intermediate.zhat1_lf_3;
    MSSshare_p &c = intermediate.c;
    MSSshare_mul_res<lf + 3> &zhat2_lf_3 = intermediate.zhat2_lf_3;
    MSSshare_mul_res<1> &bx_mul_by = intermediate.bx_mul_by;

    PI_fixed_mult_intermediate<2, lf, lf + 3> &fixed_mult_intermediate =
        intermediate.fixed_mult_intermediate;
    PI_sign_intermediate<lf + 2, (ShareValue(1) << (lf + 3))> &sign_intermediate =
        intermediate.sign_intermediate;

    // Step 1: 预处理bx_mul_by
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &bx_mul_by, &x.b, &y.b);

    // Step 2: 预处理fixed_mult
    PI_fixed_mult_preprocess(party_id, PRGs, netio, fixed_mult_intermediate, &x.t, &y.t,
                             &zhat1_lf_3);

    // Step 3: 计算zhat1_lf_2 = zhat1_lf_3的后lf+2位
    MSSshare<lf + 2> zhat1_lf_2;
#ifdef DEBUG_MODE
    zhat1_lf_2.has_preprocess = true;
#endif
    zhat1_lf_2.v1 = zhat1_lf_3.v1 & zhat1_lf_2.MASK;
    zhat1_lf_2.v2 = zhat1_lf_3.v2 & zhat1_lf_2.MASK;

    // Step 4: 预处理sign
    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate, &zhat1_lf_2, &c);

    // Step 5: 生成MSSshare的c
    MSSshare<lf + 3> c_mss;
    MSSshare_from_p(&c_mss, &c);

    // Step 6: zhat2_lf_3 = zhat1_lf_3 * (2 - c)
    MSSshare<lf + 3> two_minus_c;
#ifdef DEBUG_MODE
    two_minus_c.has_preprocess = true;
#endif
    two_minus_c.v1 = -c_mss.v1 & two_minus_c.MASK;
    two_minus_c.v2 = -c_mss.v2 & two_minus_c.MASK;

    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &zhat2_lf_3, &zhat1_lf_3, &two_minus_c);

    // Step 7: z.t = (zhat2_lf_3 >> 1) + 1
#ifdef DEBUG_MODE
    z.t.has_preprocess = true;
#endif
    z.t.v1 = (zhat2_lf_3.v1 & zhat2_lf_3.MASK) >> 1;
    z.t.v2 = (zhat2_lf_3.v2 & zhat2_lf_3.MASK) >> 1;

    // Step 8: c (in le bits)
    MSSshare<le> c_le;
#ifdef DEBUG_MODE
    c_le.has_preprocess = true;
#endif
    c_le.v1 = c_mss.v1 & c_le.MASK;
    c_le.v2 = c_mss.v2 & c_le.MASK;

    // Step 9: z.e = x.e + y.e + c_le - 2^(le-1)
    auto s1_vec = make_ptr_vec(x.e, y.e, c_le);
    auto coeff1_vec = std::vector<int>{1, 1, 1};
    MSSshare_add_res<le> z_e_tmp;
    MSSshare_add_res_preprocess_multi(party_id, &z_e_tmp, s1_vec, coeff1_vec);
    z.e = z_e_tmp;

    // Step 10: z.b = x.b + y.b - 2 * x.b * y.b
    auto s2_vec = make_ptr_vec(x.b, y.b, bx_mul_by);
    auto coeff2_vec = std::vector<int>{1, 1, -2};
    MSSshare_add_res<1> z_b_tmp;
    MSSshare_add_res_preprocess_multi(party_id, &z_b_tmp, s2_vec, coeff2_vec);
    z.b = z_b_tmp;
}

template <int lf, int le>
void PI_float_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_float_mult_intermediate<lf, le> &intermediate, FLTshare<lf, le> *input_x,
                   FLTshare<lf, le> *input_y, FLTshare<lf, le> *output_z) {
#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_float_mult: intermediate must be preprocessed before calling PI_float_mult");
    }
    if (!input_x->has_shared) {
        error("PI_float_mult: input_x must be shared before calling PI_float_mult");
    }
    if (!input_y->has_shared) {
        error("PI_float_mult: input_y must be shared before calling PI_float_mult");
    }
    if (!output_z->has_preprocess) {
        error("PI_float_mult: output_z must be preprocessed before calling PI_float_mult");
    }
    output_z->has_shared = true;
#endif

    FLTshare<lf, le> &x = *input_x;
    FLTshare<lf, le> &y = *input_y;
    FLTshare<lf, le> &z = *output_z;

    PI_fixed_mult_intermediate<2, lf, lf + 3> &fixed_mult_intermediate =
        intermediate.fixed_mult_intermediate;
    PI_sign_intermediate<lf + 2, (ShareValue(1) << (lf + 3))> &sign_intermediate =
        intermediate.sign_intermediate;
    MSSshare<lf + 3> &zhat1_lf_3 = intermediate.zhat1_lf_3;
    MSSshare_p &c = intermediate.c;
    MSSshare_mul_res<lf + 3> &zhat2_lf_3 = intermediate.zhat2_lf_3;
    MSSshare_mul_res<1> &bx_mul_by = intermediate.bx_mul_by;

    // Step 1: 计算bx_mul_by
    MSSshare_mul_res_calc_mul(party_id, netio, &bx_mul_by, &x.b, &y.b);

    // Step 2: 计算fixed_mult
    PI_fixed_mult(party_id, PRGs, netio, fixed_mult_intermediate, &x.t, &y.t, &zhat1_lf_3, false);
    ShareValue zhat1_lf_3_plain = MSSshare_recon(party_id, netio, &zhat1_lf_3);

    // Step 3: 计算zhat1_lf_2 = zhat1_lf_3的后lf+2位
    MSSshare<lf + 2> zhat1_lf_2;
#ifdef DEBUG_MODE
    zhat1_lf_2.has_preprocess = true;
    zhat1_lf_2.has_shared = true;
#endif
    zhat1_lf_2.v1 = zhat1_lf_3.v1 & zhat1_lf_2.MASK;
    zhat1_lf_2.v2 = zhat1_lf_3.v2 & zhat1_lf_2.MASK;

    // Step 4: 计算sign
    PI_sign(party_id, PRGs, netio, sign_intermediate, &zhat1_lf_2, &c);

    // Step 5: 生成MSSshare的c
    MSSshare<lf + 3> c_mss;
    MSSshare_from_p(&c_mss, &c);

    // Step 6: zhat2_lf_3 = zhat1_lf_3 * (2 - c)
    MSSshare<lf + 3> two_minus_c;
#ifdef DEBUG_MODE
    two_minus_c.has_preprocess = true;
    two_minus_c.has_shared = true;
#endif
    two_minus_c.v1 = -c_mss.v1 & two_minus_c.MASK;
    two_minus_c.v2 = -c_mss.v2 & two_minus_c.MASK;
    if (party_id == 1 || party_id == 2) {
        two_minus_c.v1 = (two_minus_c.v1 + 2) & two_minus_c.MASK;
    }

    MSSshare_mul_res_calc_mul(party_id, netio, &zhat2_lf_3, &zhat1_lf_3, &two_minus_c);

    // Step 7: z.t = (zhat2_lf_3 >> 1) + 1
#ifdef DEBUG_MODE
    z.t.has_preprocess = true;
    z.t.has_shared = true;
#endif
    z.t.v1 = (zhat2_lf_3.v1 & zhat2_lf_3.MASK) >> 1;
    z.t.v2 = (zhat2_lf_3.v2 & zhat2_lf_3.MASK) >> 1;
    if (party_id == 1 || party_id == 2) {
        z.t.v1 = (z.t.v1 + 1) & z.t.MASK;
    }

    // Step 8: c (in le bits)
    MSSshare<le> c_le;
#ifdef DEBUG_MODE
    c_le.has_preprocess = true;
    c_le.has_shared = true;
#endif
    c_le.v1 = c_mss.v1 & c_le.MASK;
    c_le.v2 = c_mss.v2 & c_le.MASK;

    // Step 9: z.e = x.e + y.e + c_le - 2^(le-1)
    auto s1_vec = make_ptr_vec(x.e, y.e, c_le);
    auto coeff1_vec = std::vector<int>{1, 1, 1};
    MSSshare_add_res<le> z_e_tmp;
    MSSshare_add_res_preprocess_multi(party_id, &z_e_tmp, s1_vec, coeff1_vec);
    MSSshare_add_res_calc_add_multi(party_id, &z_e_tmp, s1_vec, coeff1_vec);
    z.e = z_e_tmp;
    if (party_id == 1 || party_id == 2) {
        z.e.v1 = (z.e.v1 - (ShareValue(1) << (le - 1))) & z.e.MASK;
    }

    // Step 10: z.b = x.b + y.b - 2 * x.b * y.b
    auto s2_vec = make_ptr_vec(x.b, y.b, bx_mul_by);
    auto coeff2_vec = std::vector<int>{1, 1, -2};
    MSSshare_add_res<1> z_b_tmp;
    MSSshare_add_res_preprocess_multi(party_id, &z_b_tmp, s2_vec, coeff2_vec);
    MSSshare_add_res_calc_add_multi(party_id, &z_b_tmp, s2_vec, coeff2_vec);
    z.b = z_b_tmp;
}