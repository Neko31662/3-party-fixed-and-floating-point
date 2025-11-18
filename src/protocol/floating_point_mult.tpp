void PI_float_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_float_mult_intermediate &intermediate, FLTshare *input_x,
                              FLTshare *input_y, FLTshare *output_z) {
    int lf = intermediate.lf;
    int le = intermediate.le;
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
    if (input_x->lf != lf || input_x->le != le || input_y->lf != lf || input_y->le != le ||
        output_z->lf != lf || output_z->le != le) {
        error("PI_float_mult_preprocess: input_x, input_y or output_z lf/le mismatch");
    }
    intermediate.has_preprocess = true;
    output_z->has_preprocess = true;
#endif

    FLTshare &x = *input_x;
    FLTshare &y = *input_y;
    FLTshare &z = *output_z;
    MSSshare &zhat1_lf_3 = intermediate.zhat1_lf_3;
    MSSshare_p &c = intermediate.c;
    MSSshare_mul_res &zhat2_lf_3 = intermediate.zhat2_lf_3;
    MSSshare_mul_res &bx_mul_by = intermediate.bx_mul_by;

    PI_fixed_mult_intermediate &fixed_mult_intermediate = intermediate.fixed_mult_intermediate;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;

    // Step 1: 预处理bx_mul_by
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &bx_mul_by, &x.b, &y.b);

    // Step 2: 预处理fixed_mult
    PI_fixed_mult_preprocess(party_id, PRGs, netio, fixed_mult_intermediate, &x.t, &y.t,
                             &zhat1_lf_3);

    // Step 3: 计算zhat1_lf_2 = zhat1_lf_3的后lf+2位
    MSSshare zhat1_lf_2(lf + 2);
#ifdef DEBUG_MODE
    zhat1_lf_2.has_preprocess = true;
#endif
    zhat1_lf_2.v1 = zhat1_lf_3.v1 & zhat1_lf_2.MASK;
    zhat1_lf_2.v2 = zhat1_lf_3.v2 & zhat1_lf_2.MASK;

    // Step 4: 预处理sign
    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate, &zhat1_lf_2, &c);

    // Step 5: 生成MSSshare的c
    MSSshare c_mss(lf + 3);
    c_mss = c;

    // Step 6: zhat2_lf_3 = zhat1_lf_3 * (2 - c)
    MSSshare two_minus_c = c_mss * (-1);
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &zhat2_lf_3, &zhat1_lf_3, &two_minus_c);

    // Step 7: z.t = (zhat2_lf_3 >> 1) + 1
#ifdef DEBUG_MODE
    z.t.has_preprocess = true;
#endif
    z.t.v1 = (zhat2_lf_3.v1 & zhat2_lf_3.MASK) >> 1;
    z.t.v2 = (zhat2_lf_3.v2 & zhat2_lf_3.MASK) >> 1;

    // Step 8: c (in le bits)
    MSSshare c_le(le);
#ifdef DEBUG_MODE
    c_le.has_preprocess = true;
#endif
    c_le.v1 = c_mss.v1 & c_le.MASK;
    c_le.v2 = c_mss.v2 & c_le.MASK;

    // Step 9: z.e = x.e + y.e + c_le - 2^(le-1)
    z.e = x.e + y.e + c_le;

    // Step 10: z.b = x.b + y.b - 2 * x.b * y.b
    z.b = x.b + y.b - (bx_mul_by * 2);
}

void PI_float_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_float_mult_intermediate &intermediate, FLTshare *input_x, FLTshare *input_y,
                   FLTshare *output_z) {
    int lf = intermediate.lf;
    int le = intermediate.le;
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
    if (input_x->lf != lf || input_x->le != le || input_y->lf != lf || input_y->le != le ||
        output_z->lf != lf || output_z->le != le) {
        error("PI_float_mult: input_x, input_y or output_z lf/le mismatch");
    }
    output_z->has_shared = true;
#endif

    FLTshare &x = *input_x;
    FLTshare &y = *input_y;
    FLTshare &z = *output_z;
    MSSshare &zhat1_lf_3 = intermediate.zhat1_lf_3;
    MSSshare_p &c = intermediate.c;
    MSSshare_mul_res &zhat2_lf_3 = intermediate.zhat2_lf_3;
    MSSshare_mul_res &bx_mul_by = intermediate.bx_mul_by;

    PI_fixed_mult_intermediate &fixed_mult_intermediate = intermediate.fixed_mult_intermediate;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;

    // Step 1: 计算bx_mul_by
    MSSshare_mul_res_calc_mul(party_id, netio, &bx_mul_by, &x.b, &y.b);

    // Step 2: 计算fixed_mult
    PI_fixed_mult(party_id, PRGs, netio, fixed_mult_intermediate, &x.t, &y.t, &zhat1_lf_3, false);

    // Step 3: 计算zhat1_lf_2 = zhat1_lf_3的后lf+2位
    MSSshare zhat1_lf_2(lf + 2);
#ifdef DEBUG_MODE
    zhat1_lf_2.has_preprocess = true;
    zhat1_lf_2.has_shared = true;
#endif
    zhat1_lf_2.v1 = zhat1_lf_3.v1 & zhat1_lf_2.MASK;
    zhat1_lf_2.v2 = zhat1_lf_3.v2 & zhat1_lf_2.MASK;

    // Step 4: 计算sign
    PI_sign(party_id, PRGs, netio, sign_intermediate, &zhat1_lf_2, &c);

    // Step 5: 生成MSSshare的c
    MSSshare c_mss(lf + 3);
    c_mss = c;

    // Step 6: zhat2_lf_3 = zhat1_lf_3 * (2 - c)
    MSSshare two_minus_c = c_mss * (-1);
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
    MSSshare c_le(le);
#ifdef DEBUG_MODE
    c_le.has_preprocess = true;
    c_le.has_shared = true;
#endif
    c_le.v1 = c_mss.v1 & c_le.MASK;
    c_le.v2 = c_mss.v2 & c_le.MASK;

    // Step 9: z.e = x.e + y.e + c_le - 2^(le-1)
    z.e = x.e + y.e + c_le;
    MSSshare_add_plain(party_id, &z.e, -(ShareValue(1) << (le - 1)));

    // Step 10: z.b = x.b + y.b - 2 * x.b * y.b
    z.b = x.b + y.b - (bx_mul_by * 2);
}

void PI_float_mult_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                       std::vector<PI_float_mult_intermediate *> &intermediate_vec,
                       std::vector<FLTshare *> &input_x_vec, std::vector<FLTshare *> &input_y_vec,
                       std::vector<FLTshare *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != input_y_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_float_mult_vec: size of vectors mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_float_mult_intermediate &intermediate = *(intermediate_vec[i]);
        FLTshare *input_x = input_x_vec[i];
        FLTshare *input_y = input_y_vec[i];
        FLTshare *output_z = output_z_vec[i];
        int lf = intermediate.lf;
        int le = intermediate.le;
        if (!intermediate.has_preprocess) {
            error("PI_float_mult_vec: intermediate must be preprocessed before calling "
                  "PI_float_mult_vec");
        }
        if (!input_x->has_shared) {
            error("PI_float_mult_vec: input_x must be shared before calling PI_float_mult_vec");
        }
        if (!input_y->has_shared) {
            error("PI_float_mult_vec: input_y must be shared before calling PI_float_mult_vec");
        }
        if (!output_z->has_preprocess) {
            error("PI_float_mult_vec: output_z must be preprocessed before calling "
                  "PI_float_mult_vec");
        }
        if (input_x->lf != lf || input_x->le != le || input_y->lf != lf || input_y->le != le ||
            output_z->lf != lf || output_z->le != le) {
            error("PI_float_mult_vec: input_x, input_y or output_z lf/le mismatch");
        }
        output_z->has_shared = true;
    }
#endif

    int vec_size = intermediate_vec.size();

#define USE_ALL_ELEMENTS                                                                           \
    PI_float_mult_intermediate &intermediate = *(intermediate_vec[idx]);                           \
    FLTshare *input_x = input_x_vec[idx];                                                          \
    FLTshare *input_y = input_y_vec[idx];                                                          \
    FLTshare *output_z = output_z_vec[idx];                                                        \
    FLTshare &x = *input_x;                                                                        \
    FLTshare &y = *input_y;                                                                        \
    FLTshare &z = *output_z;                                                                       \
    MSSshare &zhat1_lf_3 = intermediate.zhat1_lf_3;                                                \
    MSSshare_p &c = intermediate.c;                                                                \
    MSSshare_mul_res &zhat2_lf_3 = intermediate.zhat2_lf_3;                                        \
    MSSshare_mul_res &bx_mul_by = intermediate.bx_mul_by;                                          \
    PI_fixed_mult_intermediate &fixed_mult_intermediate = intermediate.fixed_mult_intermediate;    \
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;                      \
    int &lf = intermediate.lf;                                                                     \
    int &le = intermediate.le

    // Step 1: 计算bx_mul_by
    std::vector<MSSshare_mul_res *> bx_mul_by_ptr(vec_size);
    std::vector<MSSshare *> x_b_ptr(vec_size);
    std::vector<MSSshare *> y_b_ptr(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        bx_mul_by_ptr[idx] = &bx_mul_by;
        x_b_ptr[idx] = &x.b;
        y_b_ptr[idx] = &y.b;
    }
    MSSshare_mul_res_calc_mul_vec(party_id, netio, bx_mul_by_ptr, x_b_ptr, y_b_ptr);

    // Step 2: 计算fixed_mult
    std::vector<PI_fixed_mult_intermediate *> fixed_mult_intermediate_ptr(vec_size);
    std::vector<MSSshare *> x_t_ptr(vec_size);
    std::vector<MSSshare *> y_t_ptr(vec_size);
    std::vector<MSSshare *> zhat1_lf_3_ptr(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        fixed_mult_intermediate_ptr[idx] = &fixed_mult_intermediate;
        x_t_ptr[idx] = &x.t;
        y_t_ptr[idx] = &y.t;
        zhat1_lf_3_ptr[idx] = &zhat1_lf_3;
    }
    PI_fixed_mult_vec(party_id, PRGs, netio, fixed_mult_intermediate_ptr, x_t_ptr, y_t_ptr,
                      zhat1_lf_3_ptr);

    // Step 3: 计算zhat1_lf_2 = zhat1_lf_3的后lf+2位
    std::vector<MSSshare> zhat1_lf_2_vec;
    zhat1_lf_2_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        MSSshare zhat1_lf_2(lf + 2);
#ifdef DEBUG_MODE
        zhat1_lf_2.has_preprocess = true;
        zhat1_lf_2.has_shared = true;
#endif
        zhat1_lf_2.v1 = zhat1_lf_3.v1 & zhat1_lf_2.MASK;
        zhat1_lf_2.v2 = zhat1_lf_3.v2 & zhat1_lf_2.MASK;

        zhat1_lf_2_vec.push_back(zhat1_lf_2);
    }

    // Step 4: 计算sign
    std::vector<PI_sign_intermediate *> sign_intermediate_ptr(vec_size);
    std::vector<MSSshare *> zhat1_lf_2_ptr = make_ptr_vec(zhat1_lf_2_vec);
    std::vector<MSSshare_p *> c_ptr(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        sign_intermediate_ptr[idx] = &sign_intermediate;
        c_ptr[idx] = &c;
    }
    PI_sign_vec(party_id, PRGs, netio, sign_intermediate_ptr, zhat1_lf_2_ptr, c_ptr);

    // Step 5: 生成MSSshare的c
    std::vector<MSSshare> c_mss_vec;
    std::vector<MSSshare> two_minus_c_vec;
    c_mss_vec.reserve(vec_size);
    two_minus_c_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        MSSshare c_mss(lf + 3);
        c_mss = c;

        // Step 6: zhat2_lf_3 = zhat1_lf_3 * (2 - c)
        MSSshare two_minus_c = c_mss * (-1);
        if (party_id == 1 || party_id == 2) {
            two_minus_c.v1 = (two_minus_c.v1 + 2) & two_minus_c.MASK;
        }

        c_mss_vec.push_back(c_mss);
        two_minus_c_vec.push_back(two_minus_c);
    }

    std::vector<MSSshare_mul_res *> zhat2_lf_3_ptr(vec_size);
    std::vector<MSSshare *> two_minus_c_ptr = make_ptr_vec(two_minus_c_vec);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        zhat2_lf_3_ptr[idx] = &zhat2_lf_3;
    }
    MSSshare_mul_res_calc_mul_vec(party_id, netio, zhat2_lf_3_ptr, zhat1_lf_3_ptr, two_minus_c_ptr);

    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
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
        MSSshare c_le(le);
#ifdef DEBUG_MODE
        c_le.has_preprocess = true;
        c_le.has_shared = true;
#endif
        c_le.v1 = c_mss_vec[idx].v1 & c_le.MASK;
        c_le.v2 = c_mss_vec[idx].v2 & c_le.MASK;

        // Step 9: z.e = x.e + y.e + c_le - 2^(le-1)
        z.e = x.e + y.e + c_le;
        MSSshare_add_plain(party_id, &z.e, -(ShareValue(1) << (le - 1)));

        // Step 10: z.b = x.b + y.b - 2 * x.b * y.b
        z.b = x.b + y.b - (bx_mul_by * 2);
    }

    #undef USE_ALL_ELEMENTS
}