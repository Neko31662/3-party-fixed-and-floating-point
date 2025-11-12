
void PI_trunc_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_trunc_intermediate &intermediate, MSSshare *input_x, int input_bits,
                         MSSshare *output_z) {
#ifdef DEBUG_MODE
    if (!input_x->has_preprocess) {
        error(
            "PI_trunc_preprocess: input_x must be preprocessed before calling PI_trunc_preprocess");
    }
    if (intermediate.has_preprocess) {
        error("PI_trunc_preprocess has already been called on this object");
    }
    if (output_z->has_preprocess) {
        error("PI_trunc_preprocess: output_z has already been preprocessed");
    }
    if (input_bits <= 0) {
        error("PI_trunc_preprocess: input_bits must be positive");
    }
    intermediate.input_bits = input_bits;
    intermediate.has_preprocess = true;
#endif

    int ell = intermediate.ell;
    MSSshare &x = *input_x;
    int bits = input_bits;
    MSSshare &z = *output_z;
    PI_wrap2_intermediate &wrap2_intermediate = intermediate.wrap2_intermediate;
    MSSshare_p &wrap2_res = intermediate.wrap2_res;

    // Step 1: 预处理wrap2_intermediate和wrap2_res
    PI_wrap2_preprocess(party_id, PRGs, netio, wrap2_intermediate, &x, &wrap2_res);

    // Step 2: 预处理xprime
    MSSshare xprime = x;
#ifdef DEBUG_MODE
    xprime.has_preprocess = true;
#endif
    xprime.v1 &= xprime.MASK;
    xprime.v2 &= xprime.MASK;
    xprime.v1 = (bits < ell) ? (x.v1 >> bits) : 0;
    xprime.v2 = (bits < ell) ? (x.v2 >> bits) : 0;

    // Step 3: 预处理bprime = wrap2_res * 2^{ell - bits}
    MSSshare wrap2_res_mss(ell);
    MSSshare_from_p(&wrap2_res_mss, &wrap2_res);
    MSSshare bprime = wrap2_res_mss;
#ifdef DEBUG_MODE
    bprime.has_preprocess = true;
#endif
    if (bits <= ell) {
        bprime.v1 <<= (ell - bits);
        bprime.v2 <<= (ell - bits);
        bprime.v1 &= bprime.MASK;
        bprime.v2 &= bprime.MASK;
    } else {
        bprime.v1 = 0;
        bprime.v2 = 0;
    }

    // Step 4: 预处理z = xprime - bprime
    auto s_vec = make_ptr_vec(xprime, bprime);
    auto coeff_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &z, s_vec, coeff_vec);
}

void PI_trunc(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_trunc_intermediate &intermediate, MSSshare *input_x, int input_bits,
              MSSshare *output_z) {
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_trunc: input_x must be shared before calling PI_trunc");
    }
    if (!intermediate.has_preprocess) {
        error("PI_trunc: intermediate must be preprocessed before calling PI_trunc");
    }
    if (!output_z->has_preprocess) {
        error("PI_trunc: output_z must be preprocessed before calling PI_trunc");
    }
    if (input_bits != intermediate.input_bits) {
        error("PI_trunc: input_bits mismatch");
    }
#endif

    int ell = intermediate.ell;
    MSSshare &x = *input_x;
    int bits = input_bits;
    MSSshare &z = *output_z;
    PI_wrap2_intermediate &wrap2_intermediate = intermediate.wrap2_intermediate;
    MSSshare_p &wrap2_res = intermediate.wrap2_res;

    // Step 1: 执行wrap2协议
    PI_wrap2(party_id, PRGs, netio, wrap2_intermediate, &x, &wrap2_res);

    // Step 2: 计算xprime
    MSSshare xprime = x;
#ifdef DEBUG_MODE
    xprime.has_preprocess = true;
    xprime.has_shared = true;
#endif
    xprime.v1 &= xprime.MASK;
    xprime.v2 &= xprime.MASK;
    xprime.v1 = (bits < ell) ? (x.v1 >> bits) : 0;
    xprime.v2 = (bits < ell) ? (x.v2 >> bits) : 0;

    // Step 3: 计算bprime = wrap2_res * 2^{ell - bits}
    MSSshare wrap2_res_mss(ell);
    MSSshare_from_p(&wrap2_res_mss, &wrap2_res);
    MSSshare bprime = wrap2_res_mss;
#ifdef DEBUG_MODE
    bprime.has_preprocess = true;
    bprime.has_shared = true;
#endif
    if (bits <= ell) {
        bprime.v1 <<= (ell - bits);
        bprime.v2 <<= (ell - bits);
        bprime.v1 &= bprime.MASK;
        bprime.v2 &= bprime.MASK;
    } else {
        bprime.v1 = 0;
        bprime.v2 = 0;
    }

    // Step 4: 计算z = xprime - bprime + 1
    auto s_vec = make_ptr_vec(xprime, bprime);
    auto coeff_vec = std::vector<int>{1, -1};
    MSSshare_add_res_calc_add_multi(party_id, &z, s_vec, coeff_vec);
    if (party_id == 1 || party_id == 2) {
        z.v1 = (z.v1 + 1) & z.MASK;
    }
}