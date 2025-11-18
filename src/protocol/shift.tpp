std::string to_bit_string(const LongShareValue &x, int width = 256) {
    std::string bits;
    bits.reserve(width);
    for (int i = width - 1; i >= 0; --i) {
        bits.push_back(((x >> i) & 1) ? '1' : '0');
        if (i % 16 == 0 && i != 0)
            bits.push_back(' ');
    }
    return bits;
}

void print_bits(const LongShareValue &x, int width = 256) {
    std::cout << to_bit_string(x, width) << std::endl;
}

void PI_shift_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_shift_intermediate &intermediate, MSSshare *input_x,
                         MSSshare_mul_res *output_res) {
    int &ell = intermediate.ell;
#ifdef DEBUG_MODE
    if (ell > LongShareValue_BitLength / 4) {
        error("PI_shift_preprocess only supports ell <= " +
              std::to_string(LongShareValue_BitLength / 4) + " now");
    }
    if (!input_x->has_preprocess) {
        error(
            "PI_shift_preprocess: input_x must be preprocessed before calling PI_shift_preprocess");
    }
    if (input_x->BITLEN != ell) {
        error("PI_shift_preprocess: input_x bitlength mismatch");
    }
    intermediate.has_preprocess = true;
#endif
    ADDshare_mul_res_preprocess(party_id, PRGs, netio, &intermediate.gamma1);
    ADDshare_mul_res_preprocess(party_id, PRGs, netio, &intermediate.gamma2);
    ADDshare_mul_res_preprocess(party_id, PRGs, netio, &intermediate.gamma3);
    ADDshare_mul_res_preprocess(party_id, PRGs, netio, &intermediate.d_prime);
    MSSshare_preprocess(0, party_id, PRGs, netio, &intermediate.two_pow_k);
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, output_res, input_x,
                                &intermediate.two_pow_k);
}

void PI_shift(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_shift_intermediate &intermediate, MSSshare *input_x, ADDshare<> *input_k,
              MSSshare_mul_res *output_res) {
    int &ell = intermediate.ell;
#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_shift: intermediate must be preprocessed before calling PI_shift");
    }
    if (!input_k->has_shared) {
        error("PI_shift: input_k must be shared before calling PI_shift");
    }
    if (!input_x->has_shared) {
        error("PI_shift: input_x must be shared before calling PI_shift");
    }
    if (!output_res->has_preprocess) {
        error("PI_shift: output_res must be preprocessed before calling PI_shift");
    }
    if (input_x->BITLEN != ell) {
        error("PI_shift: input_x bitlength mismatch");
    }
    if (input_k->BITLEN != LOG_1(ell)) {
        error("PI_shift: input_k bitlength mismatch");
    }
    output_res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    // Step 1: 计算di_prime = 2 ^ ki (mod 2^EXPANDED_ELL(ell) )，并设为两个加法分享
    LongShareValue di_prime = LongShareValue(1) << (input_k->v & input_k->MASK);
    ADDshare<LongShareValue> di_prime_share[2]{(EXPANDED_ELL(ell)), (EXPANDED_ELL(ell))};
#ifdef DEBUG_MODE
    for (int i = 0; i < 2; i++) {
        di_prime_share[i].has_shared = true;
    }
#endif
    if (party_id == 1) {
        di_prime_share[0].v = di_prime;
        di_prime_share[1].v = 0;
    } else {
        di_prime_share[0].v = 0;
        di_prime_share[1].v = di_prime;
    }

    // Step 2: 计算d_prime = di_prime_share[0] * di_prime_share[1]
    ADDshare_mul_res_cal_mult(party_id, netio, &intermediate.d_prime, &di_prime_share[0],
                              &di_prime_share[1]);

    // Step3: 计算gamma1
    ADDshare<> k_first_bit[2]{ell, ell};
    if (party_id == 1) {
        k_first_bit[0].v = (input_k->v & ((input_k->MASK + 1) >> 1)) ? 1 : 0;
        k_first_bit[1].v = 0;
    } else {
        k_first_bit[0].v = 0;
        k_first_bit[1].v = (input_k->v & ((input_k->MASK + 1) >> 1)) ? 1 : 0;
    }
#ifdef DEBUG_MODE
    for (int i = 0; i < 2; i++) {
        k_first_bit[i].has_shared = true;
    }
#endif
    // ADDshare_mul_res_cal_mult(party_id, netio, &intermediate.gamma1, &k_first_bit[0],
    //                           &k_first_bit[1]);

    // Step4: 计算gamma2
    ADDshare<> d_first_bit[2]{ell, ell};
    if (party_id == 1) {
        d_first_bit[0].v =
            (intermediate.d_prime.v & ((intermediate.d_prime.MASK + 1) >> (ell + 1))) ? 1 : 0;
        d_first_bit[1].v = 0;
    } else {
        d_first_bit[0].v = 0;
        d_first_bit[1].v =
            (intermediate.d_prime.v & ((intermediate.d_prime.MASK + 1) >> (ell + 1))) ? 1 : 0;
    }
#ifdef DEBUG_MODE
    for (int i = 0; i < 2; i++) {
        d_first_bit[i].has_shared = true;
    }
#endif
    // ADDshare_mul_res_cal_mult(party_id, netio, &intermediate.gamma2, &d_first_bit[0],
    //                           &d_first_bit[1]);
    auto vec0 = make_ptr_vec(intermediate.gamma1, intermediate.gamma2);
    auto vec1 = make_ptr_vec(k_first_bit[0], d_first_bit[0]);
    auto vec2 = make_ptr_vec(k_first_bit[1], d_first_bit[1]);
    ADDshare_mul_res_cal_mult_vec(party_id, netio, vec0, vec1, vec2);

    // Step5: 计算 c = k_first_bit[0] + k_first_bit[1] - gamma1
    ADDshare<> c(ell);
    c.v = (k_first_bit[0].v + k_first_bit[1].v - intermediate.gamma1.v) & c.MASK;
#ifdef DEBUG_MODE
    c.has_shared = true;
#endif

    // Step6: 计算 b = d_first_bit[0] + d_first_bit[1] - gamma2
    ADDshare<> b(ell);
    b.v = (d_first_bit[0].v + d_first_bit[1].v - intermediate.gamma2.v) & b.MASK;
#ifdef DEBUG_MODE
    b.has_shared = true;
#endif

    // Step7: 计算a0 = d_prime mod 2^ell
    // a1 = ( (d_prime >> (1<<LOG_1(ell))) + b ) mod 2^ell
    ADDshare<> a0(ell), a1(ell);
    a0.v = (intermediate.d_prime.v & a0.MASK).convert_to<ShareValue>();
    a1.v =
        (((intermediate.d_prime.v >> (1 << LOG_1(ell))).convert_to<ShareValue>() + b.v) & a1.MASK);
#ifdef DEBUG_MODE
    a0.has_shared = true;
    a1.has_shared = true;
#endif

    // Step 8: gamma3 = c * (a1-a0)
    ADDshare<> a1_minus_a0(ell);
    a1_minus_a0.v = (a1.v - a0.v) & a1_minus_a0.MASK;
#ifdef DEBUG_MODE
    a1_minus_a0.has_shared = true;
#endif
    ADDshare_mul_res_cal_mult(party_id, netio, &intermediate.gamma3, &c, &a1_minus_a0);

    // Step 9: two_pow_k = gamma3 + a0
    ADDshare<> two_pow_k_ADDshare(ell);
    two_pow_k_ADDshare.v = (intermediate.gamma3.v + a0.v) & two_pow_k_ADDshare.MASK;
#ifdef DEBUG_MODE
    two_pow_k_ADDshare.has_shared = true;
#endif

    // Step 10: P1和P2重构 two_pow_k.m = two_pow_k_ADDshare - two_pow_k.r
    ADDshare<> two_pow_k_m(ell);
    two_pow_k_m.v = (two_pow_k_ADDshare.v - intermediate.two_pow_k.v2) & two_pow_k_m.MASK;
#ifdef DEBUG_MODE
    two_pow_k_m.has_shared = true;
#endif
    intermediate.two_pow_k.v1 = ADDshare_recon(party_id, netio, &two_pow_k_m);
#ifdef DEBUG_MODE
    intermediate.two_pow_k.has_shared = true;
#endif

    // Step 11: 计算 output_res = input_x * two_pow_k
    MSSshare_mul_res_calc_mul(party_id, netio, output_res, input_x, &intermediate.two_pow_k);

    // For debug
    // auto recon1 = ADDshare_recon(party_id, netio, &di_prime_share[0]);
    // auto recon2 = ADDshare_recon(party_id, netio, &di_prime_share[1]);
    // std::cout << "log(di_prime_share[0]) recon: " << log2((uint64_t)recon1) << std::endl;
    // std::cout << "log(di_prime_share[1]) recon: " << log2((uint64_t)recon2) << std::endl;
    // auto recon3 = ADDshare_recon(party_id, netio, &intermediate.d_prime);
    // std::cout << "log(di_prime) recon: " << log2(recon3.convert_to<ShareValue>()) << std::endl;
    // auto recon4_1 = ADDshare_recon(party_id, netio, &k_first_bit[0]);
    // auto recon4_2 = ADDshare_recon(party_id, netio, &k_first_bit[1]);
    // auto reron4_3 = ADDshare_recon(party_id, netio, &intermediate.gamma1);
    // auto recon4 = ADDshare_recon(party_id, netio, &c);
    // std::cout << "k_first_bit[0] recon: " << (uint64_t)recon4_1
    //           << " ,k_first_bit[0]:" << k_first_bit[0].v << std::endl;
    // std::cout << "k_first_bit[1] recon: " << (uint64_t)recon4_2
    //           << " ,k_first_bit[1]:" << k_first_bit[1].v << std::endl;
    // std::cout << "gamma1 recon: " << (uint64_t)reron4_3 << " ,gamma1:" << intermediate.gamma1.v
    //           << std::endl;
    // std::cout << "c recon: " << (uint64_t)recon4 << std::endl << std::endl;
    // auto recon5_1 = ADDshare_recon(party_id, netio, &d_first_bit[0]);
    // auto recon5_2 = ADDshare_recon(party_id, netio, &d_first_bit[1]);
    // auto recon5_3 = ADDshare_recon(party_id, netio, &intermediate.gamma2);
    // auto recon5 = ADDshare_recon(party_id, netio, &b);
    // std::cout << "d_first_bit[0] recon: " << (uint64_t)recon5_1 << std::endl;
    // std::cout << "d_first_bit[1] recon: " << (uint64_t)recon5_2 << std::endl;
    // std::cout << "gamma2 recon: " << (uint64_t)recon5_3 << std::endl;
    // std::cout << "b recon: " << (uint64_t)recon5 << std::endl << std::endl;
    // auto recon6 = ADDshare_recon(party_id, netio, &a0);
    // std::cout << "a0 recon: " << (uint64_t)recon6 << std::endl;
    // auto recon7 = ADDshare_recon(party_id, netio, &a1);
    // std::cout << "a1 recon: " << (uint64_t)recon7 << std::endl;
    // auto recon8 = ADDshare_recon(party_id, netio, &a1_minus_a0);
    // std::cout << "a1_minus_a0 recon: " << (uint64_t)recon8 << std::endl;
    // auto recon9 = ADDshare_recon(party_id, netio, &intermediate.gamma3);
    // std::cout << "gamma3 recon: " << (uint64_t)recon9 << std::endl;
    // std::cout << std::endl;
}

void PI_shift_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  std::vector<PI_shift_intermediate *> &intermediate_vec,
                  std::vector<MSSshare *> &input_x_vec, std::vector<ADDshare<> *> &input_k_vec,
                  std::vector<MSSshare_mul_res *> &output_res_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != input_k_vec.size() ||
        intermediate_vec.size() != output_res_vec.size()) {
        error("PI_shift_vec: input vectors size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[i];
        MSSshare *input_x = input_x_vec[i];
        ADDshare<> *input_k = input_k_vec[i];
        MSSshare_mul_res *output_res = output_res_vec[i];
        int &ell = intermediate.ell;
        if (!intermediate.has_preprocess) {
            error("PI_shift_vec: intermediate must be preprocessed before calling PI_shift_vec");
        }
        if (!input_k->has_shared) {
            error("PI_shift_vec: input_k must be shared before calling PI_shift_vec");
        }
        if (!input_x->has_shared) {
            error("PI_shift_vec: input_x must be shared before calling PI_shift_vec");
        }
        if (!output_res->has_preprocess) {
            error("PI_shift_vec: output_res must be preprocessed before calling PI_shift_vec");
        }
        if (input_x->BITLEN != ell) {
            error("PI_shift_vec: input_x bitlength mismatch");
        }
        if (input_k->BITLEN != LOG_1(ell)) {
            error("PI_shift_vec: input_k bitlength mismatch");
        }
        output_res->has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }
    int vec_size = intermediate_vec.size();

    // Step 1: 计算di_prime = 2 ^ ki (mod 2^EXPANDED_ELL(ell) )，并设为两个加法分享
    std::vector<ADDshare<LongShareValue>> di_prime_share_0_vec;
    std::vector<ADDshare<LongShareValue>> di_prime_share_1_vec;
    di_prime_share_0_vec.reserve(vec_size);
    di_prime_share_1_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        MSSshare *input_x = input_x_vec[idx];
        ADDshare<> *input_k = input_k_vec[idx];
        MSSshare_mul_res *output_res = output_res_vec[idx];
        int &ell = intermediate.ell;

        LongShareValue di_prime = LongShareValue(1) << (input_k->v & input_k->MASK);
        ADDshare<LongShareValue> di_prime_share[2]{(EXPANDED_ELL(ell)), (EXPANDED_ELL(ell))};
#ifdef DEBUG_MODE
        for (int i = 0; i < 2; i++) {
            di_prime_share[i].has_shared = true;
        }
#endif
        if (party_id == 1) {
            di_prime_share[0].v = di_prime;
            di_prime_share[1].v = 0;
        } else {
            di_prime_share[0].v = 0;
            di_prime_share[1].v = di_prime;
        }
        
        di_prime_share_0_vec.push_back(di_prime_share[0]);
        di_prime_share_1_vec.push_back(di_prime_share[1]);
    }

    // Step 2: 计算d_prime = di_prime_share[0] * di_prime_share[1]
    std::vector<ADDshare_mul_res<LongShareValue> *> d_prime_vec_ptr;
    std::vector<ADDshare<LongShareValue> *> di_prime_share_0_vec_ptr;
    std::vector<ADDshare<LongShareValue> *> di_prime_share_1_vec_ptr;
    d_prime_vec_ptr.reserve(vec_size);
    di_prime_share_0_vec_ptr.reserve(vec_size);
    di_prime_share_1_vec_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        d_prime_vec_ptr.push_back(&intermediate.d_prime);
        di_prime_share_0_vec_ptr.push_back(&di_prime_share_0_vec[idx]);
        di_prime_share_1_vec_ptr.push_back(&di_prime_share_1_vec[idx]);
    }
    ADDshare_mul_res_cal_mult_vec(party_id, netio, d_prime_vec_ptr, di_prime_share_0_vec_ptr,
                                  di_prime_share_1_vec_ptr);

    std::vector<ADDshare<>> k_first_bit0_vec;
    std::vector<ADDshare<>> k_first_bit1_vec;
    std::vector<ADDshare<>> d_first_bit0_vec;
    std::vector<ADDshare<>> d_first_bit1_vec;
    k_first_bit0_vec.reserve(vec_size);
    k_first_bit1_vec.reserve(vec_size);
    d_first_bit0_vec.reserve(vec_size);
    d_first_bit1_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        MSSshare *input_x = input_x_vec[idx];
        ADDshare<> *input_k = input_k_vec[idx];
        MSSshare_mul_res *output_res = output_res_vec[idx];
        int &ell = intermediate.ell;

        // Step3: 计算gamma1
        ADDshare<> k_first_bit[2]{ell, ell};
        if (party_id == 1) {
            k_first_bit[0].v = (input_k->v & ((input_k->MASK + 1) >> 1)) ? 1 : 0;
            k_first_bit[1].v = 0;
        } else {
            k_first_bit[0].v = 0;
            k_first_bit[1].v = (input_k->v & ((input_k->MASK + 1) >> 1)) ? 1 : 0;
        }
#ifdef DEBUG_MODE
        for (int i = 0; i < 2; i++) {
            k_first_bit[i].has_shared = true;
        }
#endif

        // Step4: 计算gamma2
        ADDshare<> d_first_bit[2]{ell, ell};
        if (party_id == 1) {
            d_first_bit[0].v =
                (intermediate.d_prime.v & ((intermediate.d_prime.MASK + 1) >> (ell + 1))) ? 1 : 0;
            d_first_bit[1].v = 0;
        } else {
            d_first_bit[0].v = 0;
            d_first_bit[1].v =
                (intermediate.d_prime.v & ((intermediate.d_prime.MASK + 1) >> (ell + 1))) ? 1 : 0;
        }
#ifdef DEBUG_MODE
        for (int i = 0; i < 2; i++) {
            d_first_bit[i].has_shared = true;
        }
#endif
        k_first_bit0_vec.push_back(k_first_bit[0]);
        k_first_bit1_vec.push_back(k_first_bit[1]);
        d_first_bit0_vec.push_back(d_first_bit[0]);
        d_first_bit1_vec.push_back(d_first_bit[1]);
    }
    std::vector<ADDshare_mul_res<> *> vec0_ptr;
    std::vector<ADDshare<> *> vec1_ptr;
    std::vector<ADDshare<> *> vec2_ptr;
    vec0_ptr.reserve(vec_size * 2);
    vec1_ptr.reserve(vec_size * 2);
    vec2_ptr.reserve(vec_size * 2);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        vec0_ptr.push_back(&intermediate.gamma1);
        vec0_ptr.push_back(&intermediate.gamma2);
        vec1_ptr.push_back(&k_first_bit0_vec[idx]);
        vec1_ptr.push_back(&d_first_bit0_vec[idx]);
        vec2_ptr.push_back(&k_first_bit1_vec[idx]);
        vec2_ptr.push_back(&d_first_bit1_vec[idx]);
    }
    ADDshare_mul_res_cal_mult_vec(party_id, netio, vec0_ptr, vec1_ptr, vec2_ptr);

    std::vector<ADDshare<>> c_vec;
    std::vector<ADDshare<>> a0_vec;
    std::vector<ADDshare<>> a1_minus_a0_vec;
    c_vec.reserve(vec_size);
    a0_vec.reserve(vec_size);
    a1_minus_a0_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        MSSshare *input_x = input_x_vec[idx];
        ADDshare<> *input_k = input_k_vec[idx];
        MSSshare_mul_res *output_res = output_res_vec[idx];
        int &ell = intermediate.ell;

        // Step5: 计算 c = k_first_bit[0] + k_first_bit[1] - gamma1
        ADDshare<> c(ell);
        c.v = (k_first_bit0_vec[idx].v + k_first_bit1_vec[idx].v - intermediate.gamma1.v) & c.MASK;
#ifdef DEBUG_MODE
        c.has_shared = true;
#endif

        // Step6: 计算 b = d_first_bit[0] + d_first_bit[1] - gamma2
        ADDshare<> b(ell);
        b.v = (d_first_bit0_vec[idx].v + d_first_bit1_vec[idx].v - intermediate.gamma2.v) & b.MASK;
#ifdef DEBUG_MODE
        b.has_shared = true;
#endif

        // Step7: 计算a0 = d_prime mod 2^ell
        // a1 = ( (d_prime >> (1<<LOG_1(ell))) + b ) mod 2^ell
        ADDshare<> a0(ell), a1(ell);
        a0.v = (intermediate.d_prime.v & a0.MASK).convert_to<ShareValue>();
        a1.v = (((intermediate.d_prime.v >> (1 << LOG_1(ell))).convert_to<ShareValue>() + b.v) &
                a1.MASK);
#ifdef DEBUG_MODE
        a0.has_shared = true;
        a1.has_shared = true;
#endif

        // Step 8: gamma3 = c * (a1-a0)
        ADDshare<> a1_minus_a0(ell);
        a1_minus_a0.v = (a1.v - a0.v) & a1_minus_a0.MASK;
#ifdef DEBUG_MODE
        a1_minus_a0.has_shared = true;
#endif

        c_vec.push_back(c);
        a0_vec.push_back(a0);
        a1_minus_a0_vec.push_back(a1_minus_a0);
    }
    std::vector<ADDshare_mul_res<> *> gamma3_vec_ptr;
    std::vector<ADDshare<> *> c_vec_ptr = make_ptr_vec(c_vec);
    std::vector<ADDshare<> *> a1_minus_a0_vec_ptr = make_ptr_vec(a1_minus_a0_vec);
    gamma3_vec_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        gamma3_vec_ptr.push_back(&intermediate.gamma3);
    }
    ADDshare_mul_res_cal_mult_vec(party_id, netio, gamma3_vec_ptr, c_vec_ptr, a1_minus_a0_vec_ptr);

    std::vector<ADDshare<>> two_pow_k_m_vec;
    two_pow_k_m_vec.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        MSSshare *input_x = input_x_vec[idx];
        ADDshare<> *input_k = input_k_vec[idx];
        MSSshare_mul_res *output_res = output_res_vec[idx];
        int &ell = intermediate.ell;

        // Step 9: two_pow_k = gamma3 + a0
        ADDshare<> two_pow_k_ADDshare(ell);
        two_pow_k_ADDshare.v = (intermediate.gamma3.v + a0_vec[idx].v) & two_pow_k_ADDshare.MASK;
#ifdef DEBUG_MODE
        two_pow_k_ADDshare.has_shared = true;
#endif

        // Step 10: P1和P2重构 two_pow_k.m = two_pow_k_ADDshare - two_pow_k.r
        ADDshare<> two_pow_k_m(ell);
        two_pow_k_m.v = (two_pow_k_ADDshare.v - intermediate.two_pow_k.v2) & two_pow_k_m.MASK;
#ifdef DEBUG_MODE
        two_pow_k_m.has_shared = true;
        intermediate.two_pow_k.has_shared = true;
#endif

        two_pow_k_m_vec.push_back(two_pow_k_m);
    }
    auto two_pow_k_m_vec_ptr = make_ptr_vec(two_pow_k_m_vec);
    auto two_pow_k_m_recon_vec = ADDshare_recon_vec(party_id, netio, two_pow_k_m_vec_ptr);
    std::vector<MSSshare *> two_pow_k_vec_ptr;
    two_pow_k_vec_ptr.reserve(vec_size);
    for (int idx = 0; idx < vec_size; idx++) {
        PI_shift_intermediate &intermediate = *intermediate_vec[idx];
        MSSshare *input_x = input_x_vec[idx];
        ADDshare<> *input_k = input_k_vec[idx];
        MSSshare_mul_res *output_res = output_res_vec[idx];
        int &ell = intermediate.ell;

        intermediate.two_pow_k.v1 = two_pow_k_m_recon_vec[idx];
        two_pow_k_vec_ptr.push_back(&intermediate.two_pow_k);
    }

    // Step 11: 计算 output_res = input_x * two_pow_k
    MSSshare_mul_res_calc_mul_vec(party_id, netio, output_res_vec, input_x_vec, two_pow_k_vec_ptr);
}
