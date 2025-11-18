
void PI_eq_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                      PRGSync private_PRG, PI_eq_intermediate &intermediate, MSSshare *input_x,
                      MSSshare *input_y, MSSshare_p *output_b) {
    int &ell = intermediate.ell;
    ShareValue &k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_preprocess) {
        error("PI_eq_preprocess: input_x must be preprocessed before calling PI_eq_preprocess");
    }
    if (!input_y->has_preprocess) {
        error("PI_eq_preprocess: input_y must be preprocessed before calling PI_eq_preprocess");
    }
    if (intermediate.has_preprocess) {
        error("PI_eq_preprocess has already been called on this object");
    }
    if (output_b->has_preprocess) {
        error("PI_eq_preprocess: output_b has already been preprocessed");
    }
    if (output_b->p != k) {
        error("PI_eq_preprocess: output_b modulus mismatch");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
        error("PI_eq_preprocess: input_x or input_y BITLEN mismatch");
    }
    intermediate.has_preprocess = true;
#endif

    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare_p &b = *output_b;
    std::vector<ADDshare_p> &t_list = intermediate.t_list;
    std::vector<ADDshare_p> &rd_list = intermediate.rd_list;
    ADDshare_p &sigma = intermediate.sigma;
    ShareValue &p = intermediate.p;

    // Step 1: 预处理b
    MSSshare_p_preprocess(0, party_id, PRGs, netio, &b);

    // Step 2: P0生成sigma和t_list并分享
    if (party_id == 0) {
        private_PRG.gen_random_data(&sigma.v, sizeof(ShareValue));
        sigma.v %= sigma.p;
        t_list[sigma.v].v = 1;
    }
    ADDshare_p_share_from_store(party_id, PRGs, netio, &sigma, sigma.v);
    for (int i = 0; i < p; i++) {
        ADDshare_p_share_from_store(party_id, PRGs, netio, &t_list[i], t_list[i].v);
    }

    // Step 3: P0提取rd = ry-rx，按位分享
    if (party_id == 0) {
        ShareValue rd = 0;
        rd += y.v1 + y.v2;
        rd -= x.v1 + x.v2;
        for (int i = 0; i < ell; i++) {
            rd_list[i].v = (rd >> (ell - 1 - i)) & 1;
        }
    }
    for (int i = 0; i < ell; i++) {
        ADDshare_p_share_from_store(party_id, PRGs, netio, &rd_list[i], rd_list[i].v);
    }
}

void PI_eq(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
           PI_eq_intermediate &intermediate, MSSshare *input_x, MSSshare *input_y,
           MSSshare_p *output_b) {
    int &ell = intermediate.ell;
    ShareValue &k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_eq: input_x must be shared before calling PI_eq");
    }
    if (!input_y->has_shared) {
        error("PI_eq: input_y must be shared before calling PI_eq");
    }
    if (!intermediate.has_preprocess) {
        error("PI_eq: intermediate must be preprocessed before calling PI_eq");
    }
    if (!output_b->has_preprocess) {
        error("PI_eq: output_b must be preprocessed before calling PI_eq");
    }
    if (output_b->p != k) {
        error("PI_eq: output_b modulus mismatch");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
        error("PI_eq: input_x or input_y BITLEN mismatch");
    }
    output_b->has_shared = true;
#endif

    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare_p &b = *output_b;
    std::vector<ADDshare_p> &t_list = intermediate.t_list;
    std::vector<ADDshare_p> &rd_list = intermediate.rd_list;
    ADDshare_p &sigma = intermediate.sigma;
    ShareValue &p = intermediate.p;

    if (party_id == 0) {
        return;
    }

    // Step 1: 计算d = x - y
    MSSshare d(ell);
    d.v1 = (x.v1 - y.v1) & d.MASK;
    d.v2 = (x.v2 - y.v2) & d.MASK;
#ifdef DEBUG_MODE
    d.has_preprocess = true;
    d.has_shared = true;
#endif

    // Step 2: 按位提取md_list
    bool md_list[ell];
    for (int i = 0; i < ell; i++) {
        md_list[i] = (d.v1 >> (ell - 1 - i)) & 1;
    }

    // Step 3: 计算[delta] = [ Sigma(md_list[i] + rd_list[i] - 2*md_list[i]*rd_list[i]) ] + [sigma]
    ADDshare_p delta{p};
#ifdef DEBUG_MODE
    delta.has_shared = true;
#endif
    for (int i = 0; i < ell; i++) {
        if (party_id == 1) {
            delta.v += md_list[i];
        }
        delta.v += rd_list[i].v;
        if (md_list[i]) {
            delta.v += 2 * (delta.p - rd_list[i].v);
        }
    }
    delta.v += sigma.v;
    delta.v %= delta.p;

    // Step 4: 重构delta
    ShareValue delta_plain = ADDshare_p_recon(party_id, netio, &delta);

    // Step 5: 计算 [m_z] = [ t_list[delta_plain] ] - [r_z]
    ADDshare_p mz = t_list[delta_plain];
    mz.v = (mz.v + mz.p - b.v2) % mz.p;

    // Step 6: 重构m_z
    ShareValue mz_plain = ADDshare_p_recon(party_id, netio, &mz);
    b.v1 = mz_plain;
}

void PI_eq_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               std::vector<PI_eq_intermediate *> &intermediate_vec,
               std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare *> &input_y_vec,
               std::vector<MSSshare_p *> &output_b_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != input_y_vec.size() ||
        intermediate_vec.size() != output_b_vec.size()) {
        error("PI_eq_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_eq_intermediate &intermediate = *intermediate_vec[i];
        MSSshare *input_x = input_x_vec[i];
        MSSshare *input_y = input_y_vec[i];
        MSSshare_p *output_b = output_b_vec[i];
        int &ell = intermediate.ell;
        ShareValue &k = intermediate.k;
        if (!input_x->has_shared) {
            error("PI_eq_vec: input_x must be shared before calling PI_eq_vec");
        }
        if (!input_y->has_shared) {
            error("PI_eq_vec: input_y must be shared before calling PI_eq_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_eq_vec: intermediate must be preprocessed before calling PI_eq_vec");
        }
        if (!output_b->has_preprocess) {
            error("PI_eq_vec: output_b must be preprocessed before calling PI_eq_vec");
        }
        if (output_b->p != k) {
            error("PI_eq_vec: output_b modulus mismatch");
        }
        if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
            error("PI_eq_vec: input_x or input_y BITLEN mismatch");
        }
        output_b->has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }

    int vec_size = intermediate_vec.size();

    std::vector<ADDshare_p> delta_vec;
    delta_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_eq_intermediate &intermediate = *intermediate_vec[idx];
        MSSshare *input_x = input_x_vec[idx];
        MSSshare *input_y = input_y_vec[idx];
        MSSshare_p *output_b = output_b_vec[idx];
        MSSshare &x = *input_x;
        MSSshare &y = *input_y;
        MSSshare_p &b = *output_b;
        std::vector<ADDshare_p> &t_list = intermediate.t_list;
        std::vector<ADDshare_p> &rd_list = intermediate.rd_list;
        ADDshare_p &sigma = intermediate.sigma;
        ShareValue &p = intermediate.p;
        int &ell = intermediate.ell;
        ShareValue &k = intermediate.k;
        // Step 1: 计算d = x - y
        MSSshare d(ell);
        d.v1 = (x.v1 - y.v1) & d.MASK;
        d.v2 = (x.v2 - y.v2) & d.MASK;
#ifdef DEBUG_MODE
        d.has_preprocess = true;
        d.has_shared = true;
#endif

        // Step 2: 按位提取md_list
        bool md_list[ell];
        for (int i = 0; i < ell; i++) {
            md_list[i] = (d.v1 >> (ell - 1 - i)) & 1;
        }

        // Step 3: 计算[delta] = [ Sigma(md_list[i] + rd_list[i] - 2*md_list[i]*rd_list[i]) ] +
        // [sigma]
        ADDshare_p delta{p};
#ifdef DEBUG_MODE
        delta.has_shared = true;
#endif
        for (int i = 0; i < ell; i++) {
            if (party_id == 1) {
                delta.v += md_list[i];
            }
            delta.v += rd_list[i].v;
            if (md_list[i]) {
                delta.v += 2 * (delta.p - rd_list[i].v);
            }
        }
        delta.v += sigma.v;
        delta.v %= delta.p;

        delta_vec.push_back(delta);
    }

    // Step 4: 重构delta
    auto delta_vec_ptr = make_ptr_vec(delta_vec);
    auto delta_plain_vec = ADDshare_p_recon_vec(party_id, netio, delta_vec_ptr);

    // Step 5: 计算 [m_z] = [ t_list[delta_plain] ] - [r_z]
    std::vector<ADDshare_p> mz_vec;
    mz_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_eq_intermediate &intermediate = *intermediate_vec[idx];
        MSSshare *input_x = input_x_vec[idx];
        MSSshare *input_y = input_y_vec[idx];
        MSSshare_p *output_b = output_b_vec[idx];
        MSSshare &x = *input_x;
        MSSshare &y = *input_y;
        MSSshare_p &b = *output_b;
        std::vector<ADDshare_p> &t_list = intermediate.t_list;
        std::vector<ADDshare_p> &rd_list = intermediate.rd_list;
        ADDshare_p &sigma = intermediate.sigma;
        ShareValue &p = intermediate.p;
        int &ell = intermediate.ell;
        ShareValue &k = intermediate.k;

        ADDshare_p mz = t_list[delta_plain_vec[idx]];
        mz.v = (mz.v + mz.p - b.v2) % mz.p;

        mz_vec.push_back(mz);
    }

    // Step 6: 重构m_z
    auto mz_vec_ptr = make_ptr_vec(mz_vec);
    auto mz_plain_vec = ADDshare_p_recon_vec(party_id, netio, mz_vec_ptr);
    for (int idx = 0; idx < vec_size; idx++) {
        output_b_vec[idx]->v1 = mz_plain_vec[idx];
    }
}