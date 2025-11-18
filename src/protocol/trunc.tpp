
void PI_trunc_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_trunc_intermediate &intermediate, MSSshare *input_x, int input_bits,
                         MSSshare *output_z) {
    int ell = intermediate.ell;
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
    if (input_x->BITLEN != ell || output_z->BITLEN != ell) {
        error("PI_trunc_preprocess: input_x or output_z BITLEN mismatch");
    }
    intermediate.input_bits = input_bits;
    intermediate.has_preprocess = true;
#endif

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
    wrap2_res_mss = wrap2_res;
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
    z = xprime - bprime;
}

void PI_trunc(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_trunc_intermediate &intermediate, MSSshare *input_x, int input_bits,
              MSSshare *output_z) {
    int ell = intermediate.ell;
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
    if (input_x->BITLEN != ell || output_z->BITLEN != ell) {
        error("PI_trunc: input_x or output_z BITLEN mismatch");
    }
#endif

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
    xprime.v1 = (bits < ell) ? (x.v1 >> bits) : 0;
    xprime.v2 = (bits < ell) ? (x.v2 >> bits) : 0;

    // Step 3: 计算bprime = wrap2_res * 2^{ell - bits}
    MSSshare bprime(ell);
    bprime = wrap2_res;
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
    z = xprime - bprime;
    MSSshare_add_plain(party_id, &z, 1);
}

void PI_trunc_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  std::vector<PI_trunc_intermediate *> &intermediate_vec,
                  std::vector<MSSshare *> input_x_vec, int input_bits,
                  std::vector<MSSshare *> output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_trunc_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_trunc_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare *output_z = output_z_vec[i];
        int ell = intermediate.ell;
        if (!input_x->has_shared) {
            error("PI_trunc_vec: input_x must be shared before calling PI_trunc_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_trunc_vec: intermediate must be preprocessed before calling PI_trunc_vec");
        }
        if (!output_z->has_preprocess) {
            error("PI_trunc_vec: output_z must be preprocessed before calling PI_trunc_vec");
        }
        if (input_bits != intermediate.input_bits) {
            error("PI_trunc_vec: input_bits mismatch");
        }
        if (input_x->BITLEN != ell || output_z->BITLEN != ell) {
            error("PI_trunc_vec: input_x or output_z BITLEN mismatch");
        }
    }
#endif
    int vec_size = intermediate_vec.size();
    std::vector<PI_wrap2_intermediate *> wrap2_intermediate_vec_ptr;
    std::vector<MSSshare_p *> wrap2_res_vec_ptr;
    wrap2_intermediate_vec_ptr.reserve(vec_size);
    wrap2_res_vec_ptr.reserve(vec_size);
    for (int i = 0; i < vec_size; i++) {
        PI_trunc_intermediate &intermediate = *(intermediate_vec[i]);
        wrap2_intermediate_vec_ptr.push_back(&intermediate.wrap2_intermediate);
        wrap2_res_vec_ptr.push_back(&intermediate.wrap2_res);
    }

    // Step 1: 执行wrap2协议
    PI_wrap2_vec(party_id, PRGs, netio, wrap2_intermediate_vec_ptr, input_x_vec, wrap2_res_vec_ptr);

    for (int i = 0; i < vec_size; i++) {
        PI_trunc_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare *output_z = output_z_vec[i];

        MSSshare &x = *input_x;
        int bits = input_bits;
        MSSshare &z = *output_z;
        PI_wrap2_intermediate &wrap2_intermediate = intermediate.wrap2_intermediate;
        MSSshare_p &wrap2_res = intermediate.wrap2_res;
        int ell = intermediate.ell;

        // Step 2: 计算xprime
        MSSshare xprime = x;
#ifdef DEBUG_MODE
        xprime.has_preprocess = true;
        xprime.has_shared = true;
#endif
        xprime.v1 = (bits < ell) ? (x.v1 >> bits) : 0;
        xprime.v2 = (bits < ell) ? (x.v2 >> bits) : 0;

        // Step 3: 计算bprime = wrap2_res * 2^{ell - bits}
        MSSshare bprime(ell);
        bprime = wrap2_res;
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
        z = xprime - bprime;
        MSSshare_add_plain(party_id, &z, 1);
    }
}