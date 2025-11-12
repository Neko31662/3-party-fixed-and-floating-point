void PI_shift_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_shift_intermediate &intermediate, MSSshare *input_x,
                         MSSshare_mul_res *output_res) {
#ifdef DEBUG_MODE
    int DEBUG_ell = input_x->BITLEN;
    if (DEBUG_ell > LongShareValue_BitLength / 4) {
        error("PI_shift_preprocess only supports ell <= " +
              std::to_string(LongShareValue_BitLength / 4) + " now");
    }
    if (!input_x->has_preprocess) {
        error(
            "PI_shift_preprocess: input_x must be preprocessed before calling PI_shift_preprocess");
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
    if(LOG_1(input_x->BITLEN) != input_k->BITLEN){
        error("PI_shift: input_k bitlength mismatch");
    }
    output_res->has_shared = true;
#endif
    if (party_id == 0) {
        // Party 0 does nothing
        return;
    }

    int ell = input_x->BITLEN;

    if (party_id == 1 || party_id == 2) {
        // Step 1: 计算di_prime = 2 ^ ki (mod 2^EXPANDED_ELL(ell) )，并设为两个加法分享
        ShareValue di_prime = LongShareValue(1) << (input_k->v & input_k->MASK);
        ADDshare<LongShareValue> di_prime_share[2]{(EXPANDED_ELL(ell)), (EXPANDED_ELL(ell))};
        if (party_id == 1) {
            di_prime_share[0].v = di_prime;
            di_prime_share[1].v = 0;
        } else {
            di_prime_share[0].v = 0;
            di_prime_share[1].v = di_prime;
        }

#ifdef DEBUG_MODE
        for (int i = 0; i < 2; i++) {
            di_prime_share[i].has_shared = true;
        }
#endif

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
        // a1 = (d_prime >> (1<<LOG_1(ell)) + b ) mod 2^ell
        ADDshare<> a0(ell), a1(ell);
        a0.v = intermediate.d_prime.v & a0.MASK;
        a1.v = ((intermediate.d_prime.v >> (1 << LOG_1(ell))) + b.v) & a1.MASK;
#ifdef DEBUG_MODE
        a0.has_shared = true;
        a1.has_shared = true;
#endif

        // Step 8: gamma3 = c * (a1-a0); two_pow_k = gamma3 + a0
        ADDshare<> a1_minus_a0(ell);
        a1_minus_a0.v = (a1.v - a0.v) & a1_minus_a0.MASK;
#ifdef DEBUG_MODE
        a1_minus_a0.has_shared = true;
#endif
        ADDshare_mul_res_cal_mult(party_id, netio, &intermediate.gamma3, &c, &a1_minus_a0);
        ADDshare<> two_pow_k_ADDshare(ell);
        two_pow_k_ADDshare.v = (intermediate.gamma3.v + a0.v) & two_pow_k_ADDshare.MASK;
#ifdef DEBUG_MODE
        two_pow_k_ADDshare.has_shared = true;
#endif

        // Step 9: P1和P2重构 two_pow_k.m = two_pow_k_ADDshare - two_pow_k.r
        ShareValue two_pow_k_m =
            (two_pow_k_ADDshare.v - intermediate.two_pow_k.v2) & two_pow_k_ADDshare.MASK;
        ShareValue two_pow_k_m_recv;
        if (party_id == 1) {
            netio.send_data(2, &two_pow_k_m, two_pow_k_ADDshare.BYTELEN);
            netio.recv_data(2, &two_pow_k_m_recv, two_pow_k_ADDshare.BYTELEN);
        } else {
            netio.recv_data(1, &two_pow_k_m_recv, two_pow_k_ADDshare.BYTELEN);
            netio.send_data(1, &two_pow_k_m, two_pow_k_ADDshare.BYTELEN);
        }
        two_pow_k_m += two_pow_k_m_recv;
        intermediate.two_pow_k.v1 = two_pow_k_m & intermediate.two_pow_k.MASK;
#ifdef DEBUG_MODE
        intermediate.two_pow_k.has_shared = true;
#endif

        // Step 10: 计算 output_res = input_x * two_pow_k
        MSSshare_mul_res_calc_mul(party_id, netio, output_res, input_x, &intermediate.two_pow_k);

        // For debug
        // auto recon1 = ADDshare_recon(party_id, netio, &di_prime_share[0]);
        // auto recon2 = ADDshare_recon(party_id, netio, &di_prime_share[1]);
        // std::cout << "log(di_prime_share[0]) recon: " << log2((uint64_t)recon1) << std::endl;
        // std::cout << "log(di_prime_share[1]) recon: " << log2((uint64_t)recon2) << std::endl;
        // auto recon3 = ADDshare_recon(party_id, netio, &intermediate.d_prime);
        // std::cout << "log(di_prime) recon: " << log2(recon3) << std::endl;
        // auto recon4_1 = ADDshare_recon(party_id, netio, &k_first_bit[0]);
        // auto recon4_2 = ADDshare_recon(party_id, netio, &k_first_bit[1]);
        // auto reron4_3 = ADDshare_recon(party_id, netio, &intermediate.gamma1);
        // auto recon4 = ADDshare_recon(party_id, netio, &c);
        // std::cout << "k_first_bit[0] recon: " << (uint64_t)recon4_1
        //           << " ,k_first_bit[0]:" << k_first_bit[0].v << std::endl;
        // std::cout << "k_first_bit[1] recon: " << (uint64_t)recon4_2
        //           << " ,k_first_bit[1]:" << k_first_bit[1].v << std::endl;
        // std::cout << "gamma1 recon: " << (uint64_t)reron4_3 << " ,gamma1:" <<
        // intermediate.gamma1.v
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
}