 
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
                PI_convert_intermediate &intermediate, ADDshare<> *input_x,
                MSSshare_p *output_z) {
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

    ShareValue MASK_k = 1;
    int bitk = 1;
    while (MASK_k + 1 < k) {
        MASK_k = (MASK_k << 1) | 1;
        bitk++;
    }
    int bytek = (bitk + 7) / 8;

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
    // std::cout << "x.v:" << x.v << " " << x.v % k << std::endl;
    // std::cout << "c.v:" << c.v << " " << c.v % k << std::endl;
    // std::cout << "x[0].v:" << x0[0].v % k << std::endl;
    // std::cout << "x[1].v:" << x0[1].v % k << std::endl;

    // Step 3: P0和P2重构mz并设置z的v1
    if (party_id == 1) {
        netio.send_data(2, &mz, bytek);
        z.v1 = 0;
        netio.recv_data(2, &z.v1, bytek);
    } else if (party_id == 2) {
        z.v1 = 0;
        netio.recv_data(1, &z.v1, bytek);
        netio.send_data(1, &mz, bytek);
    }
    z.v1 = (z.v1 + mz) % k;
}
