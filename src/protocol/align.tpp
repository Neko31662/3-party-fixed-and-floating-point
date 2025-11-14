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
    ADDshare<> &zeta1_add = intermediate.zeta1_add;
    MSSshare_mul_res &xprime_mss = intermediate.xprime_mss;
    MSSshare &c_mss = intermediate.c_mss;
    MSSshare &xprime2_mss = intermediate.xprime2_mss;
    MSSshare &c_mss2 = intermediate.c_mss2;
    MSSshare &zeta1_mss = intermediate.zeta1_mss;
    MSSshare_p &b_mssp = intermediate.b_mssp;
    MSSshare_p &c_mssp = intermediate.c_mssp;
    MSSshare_p &zeta1_mssp = intermediate.zeta1_mssp;

    MSSshare &tmp1 = intermediate.tmp1;         //(xprime - xprime2)
    MSSshare_mul_res &tmp2 = intermediate.tmp2; //(c * (xprime - xprime2) )

    PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;
    PI_shift_intermediate &shift_intermediate = intermediate.shift_intermediate;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;
    PI_convert_intermediate &convert_intermediate = intermediate.convert_intermediate;

    MSSshare &temp_z_mss = intermediate.temp_z_mss;
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
    xprime2_mss = xprime_mss * 2;

    // Step 5: sign
    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate, &xprime_mss, &c_mssp);

    // Step 6: tmp1, tmp2, temp_z
    tmp1 = xprime_mss - xprime2_mss;

    MSSshare_from_p(&c_mss, &c_mssp);
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &tmp2, &c_mss, &tmp1);

    temp_z_mss = xprime2_mss + tmp2;

    // Step 7: c_mss2
    c_mss2.v1 = c_mss.v1 & c_mss2.MASK;
    c_mss2.v2 = c_mss.v2 & c_mss2.MASK;
#ifdef DEBUG_MODE
    c_mss2.has_preprocess = c_mss.has_preprocess;
    c_mss2.has_shared = c_mss.has_shared;
#endif

    // Step 8: convert
    PI_convert_preprocess(party_id, PRGs, netio, convert_intermediate, &zeta1_add, &zeta1_mssp);
    MSSshare_from_p(&zeta1_mss, &zeta1_mssp);

    // Step 9: temp_zeta
    temp_zeta_mss = zeta1_mss - c_mss2;

    // Step 10: signx
    MSSshare_p n_signx_mssp{(ShareValue(1) << ell)};
    MSSshare n_signx_mss(ell);
    MSSshare n_signx_mss2(ell2);
    MSSshare temp_z_minus_x(ell);

    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate2, &x, &signx_mssp);

    n_signx_mssp = signx_mssp * (-1);

    MSSshare_from_p(&n_signx_mss, &n_signx_mssp);

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
        error("PI_align_preprocess: input_x or output_z BITLEN mismatch");
    }
    if (output_zeta->BITLEN != ell2) {
        error("PI_align_preprocess: output_zeta BITLEN mismatch");
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
    ADDshare<> &zeta1_add = intermediate.zeta1_add;
    MSSshare_mul_res &xprime_mss = intermediate.xprime_mss;
    MSSshare &c_mss = intermediate.c_mss;
    MSSshare &xprime2_mss = intermediate.xprime2_mss;
    MSSshare &c_mss2 = intermediate.c_mss2;
    MSSshare &zeta1_mss = intermediate.zeta1_mss;
    MSSshare_p &b_mssp = intermediate.b_mssp;
    MSSshare_p &c_mssp = intermediate.c_mssp;
    MSSshare_p &zeta1_mssp = intermediate.zeta1_mssp;

    MSSshare &tmp1 = intermediate.tmp1;         //(xprime - xprime2)
    MSSshare_mul_res &tmp2 = intermediate.tmp2; //(c * (xprime - xprime2) )

    PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;
    PI_shift_intermediate &shift_intermediate = intermediate.shift_intermediate;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;
    PI_convert_intermediate &convert_intermediate = intermediate.convert_intermediate;

    MSSshare &temp_z_mss = intermediate.temp_z_mss;
    MSSshare_mul_res &temp_z2_mss = intermediate.temp_z2_mss;
    MSSshare &temp_zeta_mss = intermediate.temp_zeta_mss;
    MSSshare_p &signx_mssp = intermediate.signx_mssp;
    PI_sign_intermediate &sign_intermediate2 = intermediate.sign_intermediate2;
    ShareValue p = intermediate.p;

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
    auto L2_list_ptr = make_ptr_vec(L2_list);
    auto D_list_ptr = make_ptr_vec(D_list);
    PI_prefix_b(party_id, PRGs, netio, L2_list_ptr, &b_mssp, D_list_ptr);

    // Step 5: L1 = L1 - D
    for (int i = 0; i < ell; i++) {
        L1_list[i].v = (L1_list[i].v + p - D_list[i].v) % p;
    }

    // Step 6: detect
    auto L1_list_ptr = make_ptr_vec(L1_list);
    PI_detect(party_id, PRGs, netio, L1_list_ptr, &zeta1_addp);

    // Step 7: shift to compute xprime
    ADDshare_from_p(&zeta1_add, &zeta1_addp);
    if (party_id == 1) {
        zeta1_add.v -= 1;
        zeta1_add.v &= zeta1_add.MASK;
    }
    PI_shift(party_id, PRGs, netio, shift_intermediate, &x, &zeta1_add, &xprime_mss);
    if (party_id == 1) {
        zeta1_add.v += 1;
        zeta1_add.v &= zeta1_add.MASK;
    }

    // Step 8: xprime2
    xprime2_mss = xprime_mss * 2;

    // Step 9: sign to compute c_mss
    PI_sign(party_id, PRGs, netio, sign_intermediate, &xprime_mss, &c_mssp);
    MSSshare_from_p(&c_mss, &c_mssp);

    // Step 10: tmp1, tmp2, z
    tmp1 = xprime_mss - xprime2_mss;

    MSSshare_mul_res_calc_mul(party_id, netio, &tmp2, &c_mss, &tmp1);

    temp_z_mss = xprime2_mss + tmp2;

    // Step 11: c_mss2
    c_mss2.v1 = c_mss.v1 & c_mss2.MASK;
    c_mss2.v2 = c_mss.v2 & c_mss2.MASK;
#ifdef DEBUG_MODE
    c_mss2.has_preprocess = c_mss.has_preprocess;
    c_mss2.has_shared = c_mss.has_shared;
#endif

    // Step 12: convert zeta1_add to zeta1_mssp
    PI_convert(party_id, PRGs, netio, convert_intermediate, &zeta1_add, &zeta1_mssp);
    MSSshare_from_p(&zeta1_mss, &zeta1_mssp);

    // Step 13: zeta
    temp_zeta_mss = zeta1_mss - c_mss2;

    // Step 14: signx
    MSSshare_p n_signx_mssp{(ShareValue(1) << ell)};
    MSSshare n_signx_mss(ell);
    MSSshare n_signx_mss2(ell2);
    MSSshare temp_z_minus_x(ell);

    PI_sign(party_id, PRGs, netio, sign_intermediate2, &x, &signx_mssp);

    n_signx_mssp = signx_mssp * (-1);
    MSSshare_p_add_plain(party_id, &n_signx_mssp, 1);

    MSSshare_from_p(&n_signx_mss, &n_signx_mssp);

    n_signx_mss2.v1 = n_signx_mss.v1 & n_signx_mss2.MASK;
    n_signx_mss2.v2 = n_signx_mss.v2 & n_signx_mss2.MASK;
#ifdef DEBUG_MODE
    n_signx_mss2.has_preprocess = n_signx_mss.has_preprocess;
    n_signx_mss2.has_shared = n_signx_mss.has_shared;
#endif

    temp_z_minus_x = temp_z_mss - x;
    MSSshare_mul_res_calc_mul(party_id, netio, &temp_z2_mss, &temp_z_minus_x, &n_signx_mss);
    MSSshare_mul_res_calc_mul(party_id, netio, &zeta, &temp_zeta_mss, &n_signx_mss2);
    z = x + temp_z2_mss;
}