inline void PI_align_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                PI_align_intermediate &intermediate, MSSshare *input_x,
                                MSSshare *output_z, MSSshare_mul_res *output_zeta) {
    int ell = intermediate.ell;
    int ell2 = intermediate.ell2;
#ifdef DEBUG_MODE
    if (!input_x->has_preprocess) {
        error(
            "PI_align_preprocess: input_x must be preprocessed before calling PI_align_preprocess");
    }
    if (intermediate.has_preprocess) {
        error("PI_align_preprocess has already been called on this object");
    }
    if (output_z->has_preprocess) {
        error("PI_align_preprocess: output_z has already been preprocessed");
    }
    if (output_zeta->has_preprocess) {
        error("PI_align_preprocess: output_zeta has already been preprocessed");
    }
    if (input_x->BITLEN != ell || output_z->BITLEN != ell) {
        error("PI_align_preprocess: input_x or output_z BITLEN mismatch");
    }
    if (output_zeta->BITLEN != ell2) {
        error("PI_align_preprocess: output_zeta BITLEN mismatch");
    }
    intermediate.has_preprocess = true;
#endif
    MSSshare &x = *input_x;
    MSSshare &z = *output_z;
    MSSshare_mul_res &zeta = *output_zeta;
    std::vector<ADDshare_p> &rx_list = intermediate.rx_list;
    std::vector<ADDshare_p> &L1_list = intermediate.L1_list;
    std::vector<ADDshare_p> &L2_list = intermediate.L2_list;
    std::vector<ADDshare_p> &D_list = intermediate.D_list;
    ADDshare_p &zeta1_addp = intermediate.zeta1_addp;
    MSSshare_mul_res &xprime_mss = intermediate.xprime_mss;
    MSSshare_p &b_mssp = intermediate.b_mssp;
    MSSshare_p &c_mssp = intermediate.c_mssp;
    MSSshare_p &zeta1_mssp = intermediate.zeta1_mssp;

    MSSshare_mul_res &tmp2 = intermediate.tmp2; //(c * (xprime - xprime2) )

    PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;
    PI_shift_intermediate &shift_intermediate = intermediate.shift_intermediate;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;
    PI_convert_intermediate &convert_intermediate = intermediate.convert_intermediate;

    MSSshare_mul_res &temp_z2_mss = intermediate.temp_z2_mss;
    MSSshare &temp_zeta_mss = intermediate.temp_zeta_mss;
    MSSshare_p &signx_mssp = intermediate.signx_mssp;
    PI_sign_intermediate &sign_intermediate2 = intermediate.sign_intermediate2;

    // Step 1: 计算rx_list并分享
    ShareValue rx = (x.v1 + x.v2) & x.MASK;
    for (int i = 0; i < ell; i++) {
        ShareValue r_i = (rx >> (ell - 1 - i)) & 1;
        ADDshare_p_share_from_store(party_id, PRGs, netio, &rx_list[i], r_i);
    }

    // Step 2: wrap1
    PI_wrap1_preprocess(party_id, PRGs, netio, wrap1_intermediate, &x, &b_mssp);

    // Step 3: shift
    PI_shift_preprocess(party_id, PRGs, netio, shift_intermediate, &x, &xprime_mss);

    // Step 4: xprime2
    auto xprime2_mss = xprime_mss * 2;

    // Step 5: sign
    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate, &xprime_mss, &c_mssp);

    // Step 6: tmp1, tmp2, temp_z
    auto tmp1 = xprime_mss - xprime2_mss;

    MSSshare c_mss(ell);
    c_mss = c_mssp;
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &tmp2, &c_mss, &tmp1);

    auto temp_z_mss = xprime2_mss + tmp2;

    // Step 7: c_mss2
    MSSshare c_mss2(ell2);
    c_mss2.v1 = c_mss.v1 & c_mss2.MASK;
    c_mss2.v2 = c_mss.v2 & c_mss2.MASK;
#ifdef DEBUG_MODE
    c_mss2.has_preprocess = c_mss.has_preprocess;
    c_mss2.has_shared = c_mss.has_shared;
#endif

    // Step 8: convert
    ADDshare<> zeta1_add(LOG_1(ell));
    PI_convert_preprocess(party_id, PRGs, netio, convert_intermediate, &zeta1_add, &zeta1_mssp);
    MSSshare zeta1_mss(ell2);
    zeta1_mss = zeta1_mssp;

    // Step 9: temp_zeta
    temp_zeta_mss = zeta1_mss - c_mss2;

    // Step 10: signx
    MSSshare_p n_signx_mssp{(ShareValue(1) << ell)};
    MSSshare n_signx_mss(ell);
    MSSshare n_signx_mss2(ell2);
    MSSshare temp_z_minus_x(ell);

    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate2, &x, &signx_mssp);

    n_signx_mssp = signx_mssp * (-1);
    n_signx_mss = n_signx_mssp;
    n_signx_mss2.v1 = n_signx_mss.v1 & n_signx_mss2.MASK;
    n_signx_mss2.v2 = n_signx_mss.v2 & n_signx_mss2.MASK;
#ifdef DEBUG_MODE
    n_signx_mss2.has_preprocess = n_signx_mss.has_preprocess;
    n_signx_mss2.has_shared = n_signx_mss.has_shared;
#endif

    temp_z_minus_x = temp_z_mss - x;
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &temp_z2_mss, &temp_z_minus_x, &n_signx_mss);
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &zeta, &temp_zeta_mss, &n_signx_mss2);
    z = x + temp_z2_mss;
}

inline void PI_align(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     PI_align_intermediate &intermediate, MSSshare *input_x, MSSshare *output_z,
                     MSSshare_mul_res *output_zeta) {
    int ell = intermediate.ell;
    int ell2 = intermediate.ell2;
#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_align: PI_align_preprocess must be called before PI_align");
    }
    if (!input_x->has_shared) {
        error("PI_align: input_x must be shared before calling PI_align");
    }
    if (!output_z->has_preprocess) {
        error("PI_align: output_z must be preprocessed before calling PI_align");
    }
    if (!output_zeta->has_preprocess) {
        error("PI_align: output_zeta must be preprocessed before calling PI_align");
    }
    if (input_x->BITLEN != ell || output_z->BITLEN != ell) {
        error("PI_align: input_x or output_z BITLEN mismatch");
    }
    if (output_zeta->BITLEN != ell2) {
        error("PI_align: output_zeta BITLEN mismatch");
    }
#endif

    MSSshare &x = *input_x;
    MSSshare &z = *output_z;
    MSSshare_mul_res &zeta = *output_zeta;
    std::vector<ADDshare_p> &rx_list = intermediate.rx_list;
    std::vector<ADDshare_p> &L1_list = intermediate.L1_list;
    std::vector<ADDshare_p> &L2_list = intermediate.L2_list;
    std::vector<ADDshare_p> &D_list = intermediate.D_list;
    ADDshare_p &zeta1_addp = intermediate.zeta1_addp;
    MSSshare_mul_res &xprime_mss = intermediate.xprime_mss;
    MSSshare_p &b_mssp = intermediate.b_mssp;
    MSSshare_p &c_mssp = intermediate.c_mssp;
    MSSshare_p &zeta1_mssp = intermediate.zeta1_mssp;

    MSSshare_mul_res &tmp2 = intermediate.tmp2; //(c * (xprime - xprime2) )

    PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;
    PI_shift_intermediate &shift_intermediate = intermediate.shift_intermediate;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;
    PI_convert_intermediate &convert_intermediate = intermediate.convert_intermediate;

    MSSshare_mul_res &temp_z2_mss = intermediate.temp_z2_mss;
    MSSshare &temp_zeta_mss = intermediate.temp_zeta_mss;
    MSSshare_p &signx_mssp = intermediate.signx_mssp;
    PI_sign_intermediate &sign_intermediate2 = intermediate.sign_intermediate2;
    ShareValue &p = intermediate.p;

    // Step 1: P1和P2提取mx_list
    std::vector<bool> mx_list(ell, 0);
    if (party_id != 0) {
        ShareValue mx = x.v1 & x.MASK;
        for (int i = 0; i < ell; i++) {
            mx_list[i] = (mx >> (ell - 1 - i)) & 1;
        }
    }

    // Step 2: 计算L1_list和L2_list
    for (int i = 0; i < ell; i++) {
        L2_list[i].v = mx_list[i] * rx_list[i].v % p;
        L1_list[i].v = (rx_list[i].v - L2_list[i].v + p) % p;
        if (party_id == 1) {
            L1_list[i].v = (L1_list[i].v + mx_list[i]) % p;
        }
#ifdef DEBUG_MODE
        L2_list[i].has_shared = true;
        L1_list[i].has_shared = true;
#endif
    }

    // Step 3: wrap1
    PI_wrap1(party_id, PRGs, netio, wrap1_intermediate, &x, &b_mssp);

    // Step 4: prefix
    PI_prefix_b(party_id, PRGs, netio, L2_list, &b_mssp, D_list);

    // Step 5: L1 = L1 - D
    for (int i = 0; i < ell; i++) {
        L1_list[i].v = (L1_list[i].v + p - D_list[i].v) % p;
    }

    // Step 6: detect
    PI_detect(party_id, PRGs, netio, L1_list, &zeta1_addp);

    // Step 7: convert zeta1_add to zeta1_mssp
    ADDshare<> zeta1_add(LOG_1(ell));
    zeta1_add = zeta1_addp;
    PI_convert(party_id, PRGs, netio, convert_intermediate, &zeta1_add, &zeta1_mssp);

    // Step 8: shift to compute xprime
    if (party_id == 1) {
        zeta1_add.v -= 1;
        zeta1_add.v &= zeta1_add.MASK;
    }
    PI_shift(party_id, PRGs, netio, shift_intermediate, &x, &zeta1_add, &xprime_mss);

    // Step 9: xprime2
    auto xprime2_mss = xprime_mss * 2;

    // Step 10: sign to compute c_mss
    PI_sign(party_id, PRGs, netio, sign_intermediate, &xprime_mss, &c_mssp);
    MSSshare c_mss(ell);
    c_mss = c_mssp;

    // Step 11: tmp1, tmp2
    auto tmp1 = xprime_mss - xprime2_mss;
    MSSshare_mul_res_calc_mul(party_id, netio, &tmp2, &c_mss, &tmp1);

    // Step 12: c_mss2
    MSSshare c_mss2(ell2);
    c_mss2.v1 = c_mss.v1 & c_mss2.MASK;
    c_mss2.v2 = c_mss.v2 & c_mss2.MASK;
#ifdef DEBUG_MODE
    c_mss2.has_preprocess = c_mss.has_preprocess;
    c_mss2.has_shared = c_mss.has_shared;
#endif

    // Step 13: zeta
    MSSshare zeta1_mss(ell2);
    zeta1_mss = zeta1_mssp;
    temp_zeta_mss = zeta1_mss - c_mss2;

    // Step 14: signx
    PI_sign(party_id, PRGs, netio, sign_intermediate2, &x, &signx_mssp);

    MSSshare_p n_signx_mssp{(ShareValue(1) << ell)};
    MSSshare n_signx_mss(ell);
    MSSshare n_signx_mss2(ell2);
    MSSshare temp_z_minus_x(ell);

    n_signx_mssp = signx_mssp * (-1);
    MSSshare_p_add_plain(party_id, &n_signx_mssp, 1);

    n_signx_mss = n_signx_mssp;

    n_signx_mss2.v1 = n_signx_mss.v1 & n_signx_mss2.MASK;
    n_signx_mss2.v2 = n_signx_mss.v2 & n_signx_mss2.MASK;
#ifdef DEBUG_MODE
    n_signx_mss2.has_preprocess = n_signx_mss.has_preprocess;
    n_signx_mss2.has_shared = n_signx_mss.has_shared;
#endif

    temp_z_minus_x = xprime2_mss + tmp2 - x;
    MSSshare_mul_res_calc_mul(party_id, netio, &temp_z2_mss, &temp_z_minus_x, &n_signx_mss);
    MSSshare_mul_res_calc_mul(party_id, netio, &zeta, &temp_zeta_mss, &n_signx_mss2);
    z = x + temp_z2_mss;
}

inline void PI_align_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         std::vector<PI_align_intermediate *> &intermediate_vec,
                         std::vector<MSSshare *> &input_x_vec,
                         std::vector<MSSshare *> &output_z_vec,
                         std::vector<MSSshare_mul_res *> &output_zeta_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != output_z_vec.size() ||
        intermediate_vec.size() != output_zeta_vec.size()) {
        error("PI_align_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_align_intermediate &intermediate = *intermediate_vec[i];
        MSSshare *input_x = input_x_vec[i];
        MSSshare *output_z = output_z_vec[i];
        MSSshare_mul_res *output_zeta = output_zeta_vec[i];
        int ell = intermediate.ell;
        int ell2 = intermediate.ell2;
        if (!intermediate.has_preprocess) {
            error("PI_align: PI_align_preprocess must be called before PI_align");
        }
        if (!input_x->has_shared) {
            error("PI_align: input_x must be shared before calling PI_align");
        }
        if (!output_z->has_preprocess) {
            error("PI_align: output_z must be preprocessed before calling PI_align");
        }
        if (!output_zeta->has_preprocess) {
            error("PI_align: output_zeta must be preprocessed before calling PI_align");
        }
        if (input_x->BITLEN != ell || output_z->BITLEN != ell) {
            error("PI_align_preprocess: input_x or output_z BITLEN mismatch");
        }
        if (output_zeta->BITLEN != ell2) {
            error("PI_align_preprocess: output_zeta BITLEN mismatch");
        }
    }
#endif

#define USE_ALL_ELEMENTS                                                                           \
    PI_align_intermediate &intermediate = *intermediate_vec[idx];                                  \
    MSSshare *input_x = input_x_vec[idx];                                                          \
    MSSshare *output_z = output_z_vec[idx];                                                        \
    MSSshare_mul_res *output_zeta = output_zeta_vec[idx];                                          \
    MSSshare &x = *input_x;                                                                        \
    MSSshare &z = *output_z;                                                                       \
    MSSshare_mul_res &zeta = *output_zeta;                                                         \
    std::vector<ADDshare_p> &rx_list = intermediate.rx_list;                                       \
    std::vector<ADDshare_p> &L1_list = intermediate.L1_list;                                       \
    std::vector<ADDshare_p> &L2_list = intermediate.L2_list;                                       \
    std::vector<ADDshare_p> &D_list = intermediate.D_list;                                         \
    ADDshare_p &zeta1_addp = intermediate.zeta1_addp;                                              \
    MSSshare_mul_res &xprime_mss = intermediate.xprime_mss;                                        \
    MSSshare_p &b_mssp = intermediate.b_mssp;                                                      \
    MSSshare_p &c_mssp = intermediate.c_mssp;                                                      \
    MSSshare_p &zeta1_mssp = intermediate.zeta1_mssp;                                              \
    MSSshare_mul_res &tmp2 = intermediate.tmp2;                                                    \
    PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;                   \
    PI_shift_intermediate &shift_intermediate = intermediate.shift_intermediate;                   \
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;                      \
    PI_convert_intermediate &convert_intermediate = intermediate.convert_intermediate;             \
    MSSshare_mul_res &temp_z2_mss = intermediate.temp_z2_mss;                                      \
    MSSshare &temp_zeta_mss = intermediate.temp_zeta_mss;                                          \
    MSSshare_p &signx_mssp = intermediate.signx_mssp;                                              \
    PI_sign_intermediate &sign_intermediate2 = intermediate.sign_intermediate2;                    \
    int &ell = intermediate.ell;                                                                   \
    int &ell2 = intermediate.ell2;                                                                 \
    ShareValue &p = intermediate.p

    int vec_size = intermediate_vec.size();
    std::vector<std::vector<bool>> mx_list_vec;
    if (party_id == 1 || party_id == 2) {
        mx_list_vec.reserve(vec_size);
        for (int idx = 0; idx < vec_size; idx++) {
            USE_ALL_ELEMENTS;

            // Step 1: P1和P2提取mx_list
            std::vector<bool> mx_list(ell, 0);
            ShareValue mx = x.v1 & x.MASK;
            for (int i = 0; i < ell; i++) {
                mx_list[i] = (mx >> (ell - 1 - i)) & 1;
            }

            // Step 2: 计算L1_list和L2_list
            for (int i = 0; i < ell; i++) {
                L2_list[i].v = mx_list[i] * rx_list[i].v % p;
                L1_list[i].v = (rx_list[i].v - L2_list[i].v + p) % p;
                if (party_id == 1) {
                    L1_list[i].v = (L1_list[i].v + mx_list[i]) % p;
                }
#ifdef DEBUG_MODE
                L2_list[i].has_shared = true;
                L1_list[i].has_shared = true;
#endif
            }

            mx_list_vec.push_back(mx_list);
        }
    }

    // Step 3: wrap1
    std::vector<PI_wrap1_intermediate *> wrap1_intermediate_ptr;
    std::vector<MSSshare_p *> b_mssp_ptr;
    wrap1_intermediate_ptr.reserve(vec_size);
    b_mssp_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        wrap1_intermediate_ptr.push_back(&wrap1_intermediate);
        b_mssp_ptr.push_back(&b_mssp);
    }
    PI_wrap1_vec(party_id, PRGs, netio, wrap1_intermediate_ptr, input_x_vec, b_mssp_ptr);

    // Step 4: prefix
    std::vector<std::vector<ADDshare_p> *> L2_list_ptr;
    std::vector<std::vector<ADDshare_p> *> D_list_ptr;
    L2_list_ptr.reserve(vec_size);
    D_list_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        L2_list_ptr.push_back(&L2_list);
        D_list_ptr.push_back(&D_list);
    }
    PI_prefix_b_vec(party_id, PRGs, netio, L2_list_ptr, b_mssp_ptr, D_list_ptr);

    // Step 5: L1 = L1 - D
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        for (int i = 0; i < ell; i++) {
            L1_list[i].v = (L1_list[i].v + p - D_list[i].v) % p;
        }
    }

    // Step 6: detect
    std::vector<std::vector<ADDshare_p> *> L1_list_ptr;
    std::vector<ADDshare_p *> zeta1_addp_ptr;
    L1_list_ptr.reserve(vec_size);
    zeta1_addp_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        L1_list_ptr.push_back(&L1_list);
        zeta1_addp_ptr.push_back(&zeta1_addp);
    }
    PI_detect_vec(party_id, PRGs, netio, L1_list_ptr, zeta1_addp_ptr);

    // Step 7: convert zeta1_add to zeta1_mssp
    std::vector<ADDshare<>> zeta1_add_vec;
    std::vector<PI_convert_intermediate *> convert_intermediate_ptr;
    std::vector<MSSshare_p *> zeta1_mssp_ptr;
    zeta1_add_vec.reserve(vec_size);
    convert_intermediate_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        ADDshare<> zeta1_add(LOG_1(ell));
        zeta1_add = zeta1_addp;

        zeta1_add_vec.push_back(zeta1_add);
        convert_intermediate_ptr.push_back(&convert_intermediate);
        zeta1_mssp_ptr.push_back(&zeta1_mssp);
    }
    std::vector<ADDshare<> *> zeta1_add_ptr = make_ptr_vec(zeta1_add_vec);
    PI_convert_vec(party_id, PRGs, netio, convert_intermediate_ptr, zeta1_add_ptr, zeta1_mssp_ptr);

    // Step 8: shift to compute xprime
    std::vector<PI_shift_intermediate *> shift_intermediate_ptr;
    std::vector<MSSshare_mul_res *> xprime_mss_ptr;
    shift_intermediate_ptr.reserve(vec_size);
    xprime_mss_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        auto &zeta1_add = zeta1_add_vec[idx];
        if (party_id == 1) {
            zeta1_add.v -= 1;
            zeta1_add.v &= zeta1_add.MASK;
        }

        shift_intermediate_ptr.push_back(&shift_intermediate);
        xprime_mss_ptr.push_back(&xprime_mss);
    }
    PI_shift_vec(party_id, PRGs, netio, shift_intermediate_ptr, input_x_vec, zeta1_add_ptr,
                 xprime_mss_ptr);

    // Step 10: sign to compute c_mss
    std::vector<PI_sign_intermediate *> sign_intermediate_ptr;
    std::vector<MSSshare *> xprime_mss_ptr2;
    std::vector<MSSshare_p *> c_mssp_ptr;
    sign_intermediate_ptr.reserve(vec_size);
    xprime_mss_ptr2.reserve(vec_size);
    c_mssp_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;

        sign_intermediate_ptr.push_back(&sign_intermediate);
        xprime_mss_ptr2.push_back(&xprime_mss);
        c_mssp_ptr.push_back(&c_mssp);
    }
    PI_sign_vec(party_id, PRGs, netio, sign_intermediate_ptr, xprime_mss_ptr2, c_mssp_ptr);

    // Step 11: tmp1, tmp2, z
    std::vector<MSSshare> xprime2_mss_vec;
    std::vector<MSSshare> tmp1_vec;
    std::vector<MSSshare> c_mss_vec;
    std::vector<MSSshare_mul_res *> tmp2_ptr;
    xprime2_mss_vec.reserve(vec_size);
    tmp1_vec.reserve(vec_size);
    c_mss_vec.reserve(vec_size);
    tmp2_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;

        auto xprime2_mss = xprime_mss * 2;
        auto tmp1 = xprime_mss - xprime2_mss;
        MSSshare c_mss(ell);
        c_mss = c_mssp;

        xprime2_mss_vec.push_back(xprime2_mss);
        tmp1_vec.push_back(tmp1);
        c_mss_vec.push_back(c_mss);
        tmp2_ptr.push_back(&tmp2);
    }
    std::vector<MSSshare *> c_mss_ptr = make_ptr_vec(c_mss_vec);
    std::vector<MSSshare *> tmp1_ptr = make_ptr_vec(tmp1_vec);

    MSSshare_mul_res_calc_mul_vec(party_id, netio, tmp2_ptr, c_mss_ptr, tmp1_ptr);

    std::vector<MSSshare> c_mss2_vec;
    c_mss2_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        // Step 12: c_mss2
        MSSshare c_mss2(ell2);
        c_mss2.v1 = c_mss_vec[idx].v1 & c_mss2.MASK;
        c_mss2.v2 = c_mss_vec[idx].v2 & c_mss2.MASK;
#ifdef DEBUG_MODE
        c_mss2.has_preprocess = c_mss_vec[idx].has_preprocess;
        c_mss2.has_shared = c_mss_vec[idx].has_shared;
#endif

        // Step 13: zeta
        MSSshare zeta1_mss(ell2);
        zeta1_mss = zeta1_mssp;
        temp_zeta_mss = zeta1_mss - c_mss2;

        c_mss2_vec.push_back(c_mss2);
    }

    // Step 14: signx
    std::vector<PI_sign_intermediate *> sign_intermediate2_ptr;
    std::vector<MSSshare_p *> signx_mssp_ptr;
    sign_intermediate2_ptr.reserve(vec_size);
    signx_mssp_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        sign_intermediate2_ptr.push_back(&sign_intermediate2);
        signx_mssp_ptr.push_back(&signx_mssp);
    }
    PI_sign_vec(party_id, PRGs, netio, sign_intermediate2_ptr, input_x_vec, signx_mssp_ptr);

    std::vector<MSSshare_mul_res *> mul_res_ptr;
    std::vector<MSSshare> mul_s1_vec;
    std::vector<MSSshare> mul_s2_vec;
    mul_res_ptr.reserve(vec_size * 2);
    mul_s1_vec.reserve(vec_size * 2);
    mul_s2_vec.reserve(vec_size * 2);
    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        MSSshare_p n_signx_mssp{(ShareValue(1) << ell)};
        MSSshare n_signx_mss(ell);
        MSSshare n_signx_mss2(ell2);
        MSSshare temp_z_minus_x(ell);

        n_signx_mssp = signx_mssp * (-1);
        MSSshare_p_add_plain(party_id, &n_signx_mssp, 1);

        n_signx_mss = n_signx_mssp;

        n_signx_mss2.v1 = n_signx_mss.v1 & n_signx_mss2.MASK;
        n_signx_mss2.v2 = n_signx_mss.v2 & n_signx_mss2.MASK;
#ifdef DEBUG_MODE
        n_signx_mss2.has_preprocess = n_signx_mss.has_preprocess;
        n_signx_mss2.has_shared = n_signx_mss.has_shared;
#endif

        temp_z_minus_x = xprime2_mss_vec[idx] + tmp2 - x;

        mul_res_ptr.push_back(&temp_z2_mss);
        mul_s1_vec.push_back(temp_z_minus_x);
        mul_s2_vec.push_back(n_signx_mss);
        mul_res_ptr.push_back(&zeta);
        mul_s1_vec.push_back(temp_zeta_mss);
        mul_s2_vec.push_back(n_signx_mss2);
    }

    auto mul_s1_ptr = make_ptr_vec(mul_s1_vec);
    auto mul_s2_ptr = make_ptr_vec(mul_s2_vec);
    MSSshare_mul_res_calc_mul_vec(party_id, netio, mul_res_ptr, mul_s1_ptr, mul_s2_ptr);

    for (int idx = 0; idx < vec_size; idx++) {
        USE_ALL_ELEMENTS;
        z = x + temp_z2_mss;
    }

#undef USE_ALL_ELEMENTS
}