
void PI_eq_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                      PRGSync private_PRG, PI_eq_intermediate &intermediate, MSSshare *input_x,
                      MSSshare *input_y, MSSshare_p *output_b) {
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
    if (output_b->p != intermediate.k) {
        error("PI_eq_preprocess: output_b modulus mismatch");
    }
    intermediate.has_preprocess = true;
#endif

    int ell = input_x->BITLEN;
    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare_p &b = *output_b;
    std::vector<ADDshare_p> &t_list = intermediate.t_list;
    std::vector<ADDshare_p> &rd_list = intermediate.rd_list;
    ADDshare_p &sigma = intermediate.sigma;
    ShareValue p = intermediate.p;

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
    if (output_b->p != intermediate.k) {
        error("PI_eq: output_b modulus mismatch");
    }
    output_b->has_shared = true;
#endif

    int ell = input_x->BITLEN;
    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare_p &b = *output_b;
    std::vector<ADDshare_p> &t_list = intermediate.t_list;
    std::vector<ADDshare_p> &rd_list = intermediate.rd_list;
    ADDshare_p &sigma = intermediate.sigma;
    ShareValue p = intermediate.p;

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
    if (delta_plain >= p) {
        error("PI_eq: delta_plain >= p");
    }
    ADDshare_p mz = t_list[delta_plain];
    mz.v = (mz.v + mz.p - b.v2) % mz.p;

    // Step 6: 重构m_z
    ShareValue mz_plain = ADDshare_p_recon(party_id, netio, &mz);
    b.v1 = mz_plain;
}