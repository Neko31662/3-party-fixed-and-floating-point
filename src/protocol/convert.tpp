
void PI_convert_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                           PI_convert_intermediate &intermediate, ADDshare<> *input_x,
                           MSSshare_p *output_z) {
#ifdef DEBUG_MODE
    if (intermediate.has_preprocess) {
        error("PI_convert_preprocess has already been called on this object");
    }
    if (output_z->has_preprocess) {
        error("PI_convert_preprocess: output_z has already been preprocessed");
    }
    intermediate.has_preprocess = true;
#endif
    int ell = input_x->BITLEN;
    ShareValue k = output_z->p;

    ADDshare<> &x = *input_x;
    MSSshare_p &z = *output_z;
    ADDshare_p_mul_res &c = intermediate.c;

    MSSshare_p_preprocess(0, party_id, PRGs, netio, &z);
    ADDshare_p_mul_res_preprocess(party_id, PRGs, netio, &c);
}

void PI_convert(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                PI_convert_intermediate &intermediate, ADDshare<> *input_x, MSSshare_p *output_z) {
#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_convert: intermediate must be preprocessed before calling PI_convert");
    }
    if (!output_z->has_preprocess) {
        error("PI_convert: output_z must be preprocessed before calling PI_convert");
    }
    if (!input_x->has_shared) {
        error("PI_convert: input_x must be shared before calling PI_convert");
    }
    output_z->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    int ell = input_x->BITLEN;
    ShareValue k = output_z->p;

    ADDshare<> &x = *input_x;
    MSSshare_p &z = *output_z;
    ADDshare_p_mul_res &c = intermediate.c;
    int bytek = byte_of(k);

    // Step 1: 计算c = x分享的最高位乘积
    ADDshare_p x0[2] = {ADDshare_p(k), ADDshare_p(k)};
    if (party_id == 1) {
        x0[0].v = (x.v & x.MASK) >> (ell - 1);
        x0[1].v = 0;
    } else if (party_id == 2) {
        x0[0].v = 0;
        x0[1].v = (x.v & x.MASK) >> (ell - 1);
    }
#ifdef DEBUG_MODE
    x0[0].has_shared = true;
    x0[1].has_shared = true;
#endif
    ADDshare_p_mul_res_cal_mult(party_id, netio, &c, &x0[0], &x0[1]);

    // Step 2: 计算mz的分享
    ShareValue mz = (x.v & x.MASK);
    ShareValue tmp = (c.v - ((x.v & x.MASK) >> (ell - 1)) + k) % k;
    mz = (mz + (x.MASK % k + 1) * tmp) % k;
    mz = (mz - z.v2 + k) % k;

    // Step 3: P0和P2重构mz并设置z的v1
    z.v1 = 0;
    netio.send_data(3 - party_id, &mz, bytek);
    netio.recv_data(3 - party_id, &z.v1, bytek);
    z.v1 = (z.v1 + mz) % k;
}

void PI_convert_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                    std::vector<PI_convert_intermediate *> &intermediate_vec,
                    std::vector<ADDshare<> *> &input_x_vec,
                    std::vector<MSSshare_p *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != output_z_vec.size()) {
        error("PI_convert_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_convert_intermediate &intermediate = *intermediate_vec[i];
        ADDshare<> *input_x = input_x_vec[i];
        MSSshare_p *output_z = output_z_vec[i];
        if (!intermediate.has_preprocess) {
            error("PI_convert: intermediate must be preprocessed before calling PI_convert");
        }
        if (!output_z->has_preprocess) {
            error("PI_convert: output_z must be preprocessed before calling PI_convert");
        }
        if (!input_x->has_shared) {
            error("PI_convert: input_x must be shared before calling PI_convert");
        }
        output_z->has_shared = true;
    }
#endif

    if (party_id == 0) {
        return;
    }
    int vec_size = intermediate_vec.size();

    std::vector<ADDshare_p> x00_vec, x01_vec;
    x00_vec.reserve(vec_size);
    x01_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_convert_intermediate &intermediate = *intermediate_vec[idx];
        ADDshare<> *input_x = input_x_vec[idx];
        MSSshare_p *output_z = output_z_vec[idx];
        ADDshare<> &x = *input_x;
        MSSshare_p &z = *output_z;
        ADDshare_p_mul_res &c = intermediate.c;
        ShareValue &k = output_z->p;
        int &ell = input_x->BITLEN;

        // Step 1: 计算c = x分享的最高位乘积
        ADDshare_p x0[2] = {ADDshare_p(k), ADDshare_p(k)};
        if (party_id == 1) {
            x0[0].v = (x.v & x.MASK) >> (ell - 1);
            x0[1].v = 0;
        } else if (party_id == 2) {
            x0[0].v = 0;
            x0[1].v = (x.v & x.MASK) >> (ell - 1);
        }
#ifdef DEBUG_MODE
        x0[0].has_shared = true;
        x0[1].has_shared = true;
#endif

        x00_vec.push_back(x0[0]);
        x01_vec.push_back(x0[1]);
    }
    std::vector<ADDshare_p_mul_res *> c_ptr;
    c_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_convert_intermediate &intermediate = *intermediate_vec[idx];
        c_ptr.push_back(&intermediate.c);
    }
    auto x00_ptr = make_ptr_vec(x00_vec);
    auto x01_ptr = make_ptr_vec(x01_vec);
    ADDshare_p_mul_res_cal_mult_vec(party_id, netio, c_ptr, x00_ptr, x01_ptr);

    for (int idx = 0; idx < vec_size; idx++) {
        PI_convert_intermediate &intermediate = *intermediate_vec[idx];
        ADDshare<> *input_x = input_x_vec[idx];
        MSSshare_p *output_z = output_z_vec[idx];
        ADDshare<> &x = *input_x;
        MSSshare_p &z = *output_z;
        ADDshare_p_mul_res &c = intermediate.c;
        ShareValue &k = output_z->p;
        int &ell = input_x->BITLEN;
        int bytek = byte_of(k);

        // Step 2: 计算mz的分享
        ShareValue mz = (x.v & x.MASK);
        ShareValue tmp = (c.v - ((x.v & x.MASK) >> (ell - 1)) + k) % k;
        mz = (mz + (x.MASK % k + 1) * tmp) % k;
        mz = (mz - z.v2 + k) % k;

        // Step 3: P0和P2重构mz并设置z的v1
        z.v1 = mz;
        netio.store_data(3 - party_id, &mz, bytek);
    }
    netio.send_stored_data(3 - party_id);

    for (int idx = 0; idx < vec_size; idx++) {
        PI_convert_intermediate &intermediate = *intermediate_vec[idx];
        ADDshare<> *input_x = input_x_vec[idx];
        MSSshare_p *output_z = output_z_vec[idx];
        ADDshare<> &x = *input_x;
        MSSshare_p &z = *output_z;
        ADDshare_p_mul_res &c = intermediate.c;
        ShareValue &k = output_z->p;
        int bytek = byte_of(k);

        ShareValue tmp = 0;
        netio.recv_data(3 - party_id, &tmp, bytek);
        z.v1 = (z.v1 + tmp) % k;
    }
}
