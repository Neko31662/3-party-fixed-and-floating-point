void PI_wrap1_spec_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_wrap1_spec_intermediate &intermediate, MSSshare *input_x,
                              MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (intermediate.has_preprocess) {
        error("PI_wrap1_spec_preprocess has already been called on this object");
    }
    if (!input_x->has_preprocess) {
        error("PI_wrap1_spec_preprocess: input_x must be preprocessed before calling "
              "PI_wrap1_spec_preprocess");
    }
    if (output_z->has_preprocess) {
        error("PI_wrap1_spec_preprocess: output_z has already been preprocessed");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap1_spec_preprocess: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap1_spec_preprocess: input_x bitlen mismatch");
    }
    intermediate.has_preprocess = true;
    intermediate.rx0.has_shared = true;
#endif

    MSSshare &x = *input_x;
    MSSshare_p &z = *output_z;
    ADDshare_p &rx0 = intermediate.rx0;

    MSSshare_p_preprocess(0, party_id, PRGs, netio, output_z);

    bool plain_rx0;
    ShareValue r = x.v1 + x.v2;
    plain_rx0 = (r >> (ell - 1) & 1);
    ADDshare_p_share_from_store(party_id, PRGs, netio, &rx0, plain_rx0 ? 1 : 0);
}

void PI_wrap2_spec_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_wrap2_spec_intermediate &intermediate, MSSshare *input_x,
                              MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (intermediate.has_preprocess) {
        error("PI_wrap2_spec_preprocess has already been called on this object");
    }
    if (!input_x->has_preprocess) {
        error("PI_wrap2_spec_preprocess: input_x must be preprocessed before calling "
              "PI_wrap2_spec_preprocess");
    }
    if (output_z->has_preprocess) {
        error("PI_wrap2_spec_preprocess: output_z has already been preprocessed");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap2_spec_preprocess: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap2_spec_preprocess: input_x bitlen mismatch");
    }
    intermediate.has_preprocess = true;
    intermediate.b.has_shared = true;
#endif

    MSSshare &x = *input_x;
    MSSshare_p &z = *output_z;
    ADDshare_p &b = intermediate.b;

    PI_wrap1_spec_preprocess(party_id, PRGs, netio, intermediate.wrap1_intermediate, input_x,
                             output_z);

    bool plain_b;
    ShareValue r = x.v1 + x.v2;
    if (ell == ShareValue_BitLength) {
        plain_b = (r < x.v1);
    } else {
        plain_b = (r >> ell);
    }
    ADDshare_p_share_from_store(party_id, PRGs, netio, &b, plain_b ? 1 : 0);
}

void PI_wrap1_spec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_wrap1_spec_intermediate &intermediate, MSSshare *input_x,
                   MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_wrap1_spec: input_x must be shared before calling PI_wrap1_spec");
    }
    if (!intermediate.has_preprocess) {
        error("PI_wrap1_spec: intermediate must be preprocessed before calling PI_wrap1_spec");
    }
    if (!output_z->has_preprocess) {
        error("PI_wrap1_spec: output_z must be preprocessed before calling PI_wrap1_spec");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap1_spec: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap1_spec: input_x bitlen mismatch");
    }
    output_z->has_shared = true;
#endif

    if (party_id == 0) {
        return;
    }

    MSSshare &x = *input_x;
    MSSshare_p &z = *output_z;
    ADDshare_p &rx0 = intermediate.rx0;
    int bytek = byte_of(k);

    ADDshare_p mz = rx0;
    bool plain_mx0 = (x.v1 >> (ell - 1)) & 1;
    if (plain_mx0) {
        mz.v = (mz.v - rx0.v + mz.p) % mz.p;
    }
    if (party_id == 1) {
        mz.v = (mz.v + plain_mx0) % mz.p;
    }
    mz.v = (mz.v - z.v2 + z.p) % mz.p;
    z.v1 = ADDshare_p_recon(party_id, netio, &mz);
}

void PI_wrap2_spec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_wrap2_spec_intermediate &intermediate, MSSshare *input_x,
                   MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_wrap2_spec: input_x must be shared before calling PI_wrap2_spec");
    }
    if (!intermediate.has_preprocess) {
        error("PI_wrap2_spec: intermediate must be preprocessed before calling PI_wrap2_spec");
    }
    if (!output_z->has_preprocess) {
        error("PI_wrap2_spec: output_z must be preprocessed before calling PI_wrap2_spec");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap2_spec: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap2_spec: input_x bitlen mismatch");
    }
    output_z->has_shared = true;
#endif

    if (party_id == 0) {
        return;
    }

    MSSshare &x = *input_x;
    MSSshare_p &z = *output_z;
    ADDshare_p &rx0 = intermediate.wrap1_intermediate.rx0;
    ADDshare_p &b = intermediate.b;
    int bytek = byte_of(k);

    ADDshare_p mz = rx0;
    bool plain_mx0 = (x.v1 >> (ell - 1)) & 1;
    if (plain_mx0) {
        mz.v = (mz.v - rx0.v + mz.p) % mz.p;
    }
    if (party_id == 1) {
        mz.v = (mz.v + plain_mx0) % mz.p;
    }
    mz.v = (mz.v - z.v2 + z.p) % mz.p;
    mz.v = (mz.v + b.v) % mz.p;
    z.v1 = ADDshare_p_recon(party_id, netio, &mz);
}

void PI_wrap1_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_wrap1_intermediate &intermediate, MSSshare *input_x,
                         MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (intermediate.has_preprocess) {
        error("PI_wrap1_preprocess has already been called on this object");
    }
    if (!input_x->has_preprocess) {
        error(
            "PI_wrap1_preprocess: input_x must be preprocessed before calling PI_wrap1_preprocess");
    }
    if (output_z->has_preprocess) {
        error("PI_wrap1_preprocess: output_z has already been preprocessed");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap1_preprocess: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap1_preprocess: input_x bitlen mismatch");
    }
    intermediate.has_preprocess = true;
    intermediate.tmpx.has_preprocess = true;
#endif

    MSSshare &x = *input_x;
    MSSshare &tmpx = intermediate.tmpx;
    MSSshare_p &z = *output_z;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;
    MSSshare_p &b = intermediate.b;
    MSSshare_p &c = intermediate.c;
    MSSshare_p_mul_res &b_mul_c = intermediate.b_mul_c;

    tmpx.v1 = x.v1;
    tmpx.v2 = x.v2;

    bool plain_c;
    if (party_id == 0) {
        ShareValue r = x.v1 + x.v2;
        if (ell == ShareValue_BitLength) {
            plain_c = (r < x.v1);
        } else {
            plain_c = (r >> ell);
        }
    }
    MSSshare_p_preprocess(0, party_id, PRGs, netio, &c);
    MSSshare_p_share_from_store(party_id, netio, &c, plain_c);
    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate, &tmpx, &b);
    MSSshare_p_mul_res_preprocess(party_id, PRGs, netio, &b_mul_c, &b, &c);

    z = b + c - (b_mul_c * 2);
}

void PI_wrap2_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_wrap2_intermediate &intermediate, MSSshare *input_x,
                         MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (intermediate.has_preprocess) {
        error("PI_wrap2_preprocess has already been called on this object");
    }
    if (!input_x->has_preprocess) {
        error(
            "PI_wrap2_preprocess: input_x must be preprocessed before calling PI_wrap2_preprocess");
    }
    if (output_z->has_preprocess) {
        error("PI_wrap2_preprocess: output_z has already been preprocessed");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap2_preprocess: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap2_preprocess: input_x bitlen mismatch");
    }
    intermediate.has_preprocess = true;
#endif

    MSSshare &x = *input_x;
    MSSshare_p &z = *output_z;
    PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;
    MSSshare_p &d = intermediate.d;

    PI_wrap1_preprocess(party_id, PRGs, netio, wrap1_intermediate, &x, &d);
    z = d + wrap1_intermediate.c;
}

void PI_wrap1(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_wrap1_intermediate &intermediate, MSSshare *input_x, MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_wrap1: input_x must be shared before calling PI_wrap1");
    }
    if (!intermediate.has_preprocess) {
        error("PI_wrap1: intermediate must be preprocessed before calling PI_wrap1");
    }
    if (!output_z->has_preprocess) {
        error("PI_wrap1: output_z must be preprocessed before calling PI_wrap1");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap1: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap1: input_x bitlen mismatch");
    }
    intermediate.tmpx.has_shared = true;
#endif

    MSSshare &x = *input_x;
    MSSshare &tmpx = intermediate.tmpx;
    MSSshare_p &z = *output_z;
    PI_sign_intermediate &sign_intermediate = intermediate.sign_intermediate;
    MSSshare_p &b = intermediate.b;
    MSSshare_p &c = intermediate.c;
    MSSshare_p_mul_res &b_mul_c = intermediate.b_mul_c;

    tmpx.v1 = x.v1;
    tmpx.v2 = x.v2;

    PI_sign(party_id, PRGs, netio, sign_intermediate, &tmpx, &b);
    MSSshare_p_mul_res_calc_mul(party_id, netio, &b_mul_c, &b, &c);
    z = b + c - (b_mul_c * 2);
}

void PI_wrap2(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_wrap2_intermediate &intermediate, MSSshare *input_x, MSSshare_p *output_z) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_wrap2: input_x must be shared before calling PI_wrap2");
    }
    if (!intermediate.has_preprocess) {
        error("PI_wrap2: intermediate must be preprocessed before calling PI_wrap2");
    }
    if (!output_z->has_preprocess) {
        error("PI_wrap2: output_z must be preprocessed before calling PI_wrap2");
    }
    if (output_z->p != intermediate.k) {
        error("PI_wrap2: output_z modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_wrap2: input_x bitlen mismatch");
    }
#endif

    MSSshare &x = *input_x;
    MSSshare_p &z = *output_z;
    PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;
    MSSshare_p &d = intermediate.d;

    PI_wrap1(party_id, PRGs, netio, wrap1_intermediate, &x, &d);
    z = d + wrap1_intermediate.c;
}

void PI_wrap1_spec_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                       std::vector<PI_wrap1_spec_intermediate *> &intermediate_vec,
                       std::vector<MSSshare *> &input_x_vec,
                       std::vector<MSSshare_p *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_wrap1_spec_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_wrap1_spec_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare_p *output_z = output_z_vec[i];
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;
        if (!input_x->has_shared) {
            error("PI_wrap1_spec_vec: input_x must be shared before calling PI_wrap1_spec_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_wrap1_spec_vec: intermediate must be preprocessed before calling "
                  "PI_wrap1_spec_vec");
        }
        if (!output_z->has_preprocess) {
            error("PI_wrap1_spec_vec: output_z must be preprocessed before calling "
                  "PI_wrap1_spec_vec");
        }
        if (output_z->p != intermediate.k) {
            error("PI_wrap1_spec_vec: output_z modulus mismatch");
        }
        if (input_x->BITLEN != ell) {
            error("PI_wrap1_spec_vec: input_x bitlen mismatch");
        }
        output_z->has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }
    int vec_size = intermediate_vec.size();
    std::vector<ADDshare_p> mz_vec;
    mz_vec.reserve(vec_size);
    for (int i = 0; i < vec_size; i++) {
        PI_wrap1_spec_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare_p *output_z = output_z_vec[i];
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;

        MSSshare &x = *input_x;
        MSSshare_p &z = *output_z;
        ADDshare_p &rx0 = intermediate.rx0;
        int bytek = byte_of(k);

        ADDshare_p mz = rx0;
        bool plain_mx0 = (x.v1 >> (ell - 1)) & 1;
        if (plain_mx0) {
            mz.v = (mz.v - rx0.v + mz.p) % mz.p;
        }
        if (party_id == 1) {
            mz.v = (mz.v + plain_mx0) % mz.p;
        }
        mz.v = (mz.v - z.v2 + z.p) % mz.p;
        mz_vec.push_back(mz);
    }
    auto mz_vec_ptr = make_ptr_vec(mz_vec);
    auto recon_vec = ADDshare_p_recon_vec(party_id, netio, mz_vec_ptr);
    for (int i = 0; i < vec_size; i++) {
        output_z_vec[i]->v1 = recon_vec[i];
    }
}

void PI_wrap2_spec_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                       std::vector<PI_wrap2_spec_intermediate *> &intermediate_vec,
                       std::vector<MSSshare *> &input_x_vec,
                       std::vector<MSSshare_p *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_wrap2_spec_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_wrap2_spec_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare_p *output_z = output_z_vec[i];
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;
        if (!input_x->has_shared) {
            error("PI_wrap2_spec_vec: input_x must be shared before calling PI_wrap2_spec_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_wrap2_spec_vec: intermediate must be preprocessed before calling "
                  "PI_wrap2_spec_vec");
        }
        if (!output_z->has_preprocess) {
            error("PI_wrap2_spec_vec: output_z must be preprocessed before calling "
                  "PI_wrap2_spec_vec");
        }
        if (output_z->p != intermediate.k) {
            error("PI_wrap2_spec_vec: output_z modulus mismatch");
        }
        if (input_x->BITLEN != ell) {
            error("PI_wrap2_spec_vec: input_x bitlen mismatch");
        }
        output_z->has_shared = true;
    }
#endif

    if (party_id == 0) {
        return;
    }
    int vec_size = intermediate_vec.size();
    std::vector<ADDshare_p> mz_vec;
    mz_vec.reserve(vec_size);
    for (int i = 0; i < vec_size; i++) {
        PI_wrap2_spec_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare_p *output_z = output_z_vec[i];
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;
        MSSshare &x = *input_x;
        MSSshare_p &z = *output_z;
        ADDshare_p &rx0 = intermediate.wrap1_intermediate.rx0;
        ADDshare_p &b = intermediate.b;
        int bytek = byte_of(k);

        ADDshare_p mz = rx0;
        bool plain_mx0 = (x.v1 >> (ell - 1)) & 1;
        if (plain_mx0) {
            mz.v = (mz.v - rx0.v + mz.p) % mz.p;
        }
        if (party_id == 1) {
            mz.v = (mz.v + plain_mx0) % mz.p;
        }
        mz.v = (mz.v - z.v2 + z.p) % mz.p;
        mz.v = (mz.v + b.v) % mz.p;
        mz_vec.push_back(mz);
    }
    auto mz_vec_ptr = make_ptr_vec(mz_vec);
    auto recon_vec = ADDshare_p_recon_vec(party_id, netio, mz_vec_ptr);
    for (int i = 0; i < vec_size; i++) {
        output_z_vec[i]->v1 = recon_vec[i];
    }
}

void PI_wrap1_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  std::vector<PI_wrap1_intermediate *> &intermediate_vec,
                  std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare_p *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_wrap1_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_wrap1_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare_p *output_z = output_z_vec[i];
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;
        if (!input_x->has_shared) {
            error("PI_wrap1_vec: input_x must be shared before calling PI_wrap1_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_wrap1_vec: intermediate must be preprocessed before calling PI_wrap1_vec");
        }
        if (!output_z->has_preprocess) {
            error("PI_wrap1_vec: output_z must be preprocessed before calling PI_wrap1_vec");
        }
        if (output_z->p != intermediate.k) {
            error("PI_wrap1_vec: output_z modulus mismatch");
        }
        if (input_x->BITLEN != ell) {
            error("PI_wrap1_vec: input_x bitlen mismatch");
        }
        intermediate.tmpx.has_shared = true;
    }
#endif

    int vec_size = intermediate_vec.size();
    for (int i = 0; i < vec_size; i++) {
        intermediate_vec[i]->tmpx.v1 = input_x_vec[i]->v1;
        intermediate_vec[i]->tmpx.v2 = input_x_vec[i]->v2;
    }

    std::vector<MSSshare *> tmpx_vec_ptr;
    std::vector<MSSshare_p *> b_vec_ptr;
    std::vector<PI_sign_intermediate *> sign_intermediate_vec_ptr;
    for (int i = 0; i < vec_size; i++) {
        PI_wrap1_intermediate &intermediate = *(intermediate_vec[i]);
        tmpx_vec_ptr.push_back(&intermediate.tmpx);
        b_vec_ptr.push_back(&intermediate.b);
        sign_intermediate_vec_ptr.push_back(&intermediate.sign_intermediate);
    }
    PI_sign_vec(party_id, PRGs, netio, sign_intermediate_vec_ptr, tmpx_vec_ptr, b_vec_ptr);

    std::vector<MSSshare_p *> c_vec_ptr;
    std::vector<MSSshare_p_mul_res *> b_mul_c_vec_ptr;
    for (int i = 0; i < vec_size; i++) {
        PI_wrap1_intermediate &intermediate = *(intermediate_vec[i]);
        c_vec_ptr.push_back(&intermediate.c);
        b_mul_c_vec_ptr.push_back(&intermediate.b_mul_c);
    }
    MSSshare_p_mul_res_calc_mul_vec(party_id, netio, b_mul_c_vec_ptr, b_vec_ptr, c_vec_ptr);

    for (int i = 0; i < vec_size; i++) {
        PI_wrap1_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare_p &z = *(output_z_vec[i]);
        MSSshare_p &b = intermediate.b;
        MSSshare_p &c = intermediate.c;
        MSSshare_p_mul_res &b_mul_c = intermediate.b_mul_c;
        z = b + c - (b_mul_c * 2);
    }
}

void PI_wrap2_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  std::vector<PI_wrap2_intermediate *> &intermediate_vec,
                  std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare_p *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_wrap2_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_wrap2_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare *input_x = input_x_vec[i];
        MSSshare_p *output_z = output_z_vec[i];
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;
        if (!input_x->has_shared) {
            error("PI_wrap2_vec: input_x must be shared before calling PI_wrap2_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_wrap2_vec: intermediate must be preprocessed before calling PI_wrap2_vec");
        }
        if (!output_z->has_preprocess) {
            error("PI_wrap2_vec: output_z must be preprocessed before calling PI_wrap2_vec");
        }
        if (output_z->p != intermediate.k) {
            error("PI_wrap2_vec: output_z modulus mismatch");
        }
        if (input_x->BITLEN != ell) {
            error("PI_wrap2_vec: input_x bitlen mismatch");
        }
    }
#endif
    int vec_size = intermediate_vec.size();
    std::vector<PI_wrap1_intermediate *> wrap1_intermediate_vec_ptr;
    std::vector<MSSshare_p *> d_vec_ptr;
    for (int i = 0; i < vec_size; i++) {
        PI_wrap2_intermediate &intermediate = *(intermediate_vec[i]);
        wrap1_intermediate_vec_ptr.push_back(&intermediate.wrap1_intermediate);
        d_vec_ptr.push_back(&intermediate.d);
    }
    
    PI_wrap1_vec(party_id, PRGs, netio, wrap1_intermediate_vec_ptr, input_x_vec, d_vec_ptr);

    for (int i = 0; i < vec_size; i++) {
        PI_wrap2_intermediate &intermediate = *(intermediate_vec[i]);
        MSSshare_p &z = *(output_z_vec[i]);
        MSSshare_p &d = intermediate.d;
        PI_wrap1_intermediate &wrap1_intermediate = intermediate.wrap1_intermediate;
        z = d + wrap1_intermediate.c;
    }
}