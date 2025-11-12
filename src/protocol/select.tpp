
void PI_select_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                          PI_select_intermediate &intermediate, MSSshare *input_x,
                          MSSshare *input_y, MSSshare *input_b, MSSshare *output_z) {
    int ell = input_x->BITLEN;
#ifdef DEBUG_MODE
    if (!input_x->has_preprocess) {
        error("PI_select_preprocess: input_x must be preprocessed before calling "
              "PI_select_preprocess");
    }
    if (!input_y->has_preprocess) {
        error("PI_select_preprocess: input_y must be preprocessed before calling "
              "PI_select_preprocess");
    }
    if (!input_b->has_preprocess) {
        error("PI_select_preprocess: input_b must be preprocessed before calling "
              "PI_select_preprocess");
    }
    if (intermediate.has_preprocess) {
        error("PI_select_preprocess has already been called on this object");
    }
    if (output_z->has_preprocess) {
        error("PI_select_preprocess: output_z has already been preprocessed");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell || output_z->BITLEN != ell) {
        error("PI_select_preprocess: input bit-length mismatch");
    }
    if (input_b->BITLEN != 1) {
        error("PI_select_preprocess: input_b must be a 1-bit share");
    }
    intermediate.has_preprocess = true;
#endif

    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare &b = *input_b;
    MSSshare &z = *output_z;
    MSSshare &rb = intermediate.rb;
    MSSshare_mul_res &mb_mul_rb = intermediate.mb_mul_rb;
    MSSshare_mul_res &bprime_mul_dif = intermediate.bprime_mul_dif;

    // Step 1: P0分享rb
    ShareValue rb_plain = (b.v1 + b.v2) & b.MASK;
    MSSshare_preprocess(0, party_id, PRGs, netio, &rb);
    MSSshare_share_from_store(party_id, netio, &rb, rb_plain);

    // Step 2: 生成MSSshare mb，r1=r2=0
    MSSshare mb(ell);
#ifdef DEBUG_MODE
    mb.has_preprocess = true;
#endif

    // Step 3: 预处理 mb*rb
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &mb_mul_rb, &mb, &rb);

    // Step 4: 生成bprime = mb + rb -2*mb*rb, dif = x - y
    MSSshare bprime(ell);
    auto s1_vec = make_ptr_vec(mb, rb, mb_mul_rb);
    auto coeff1_vec = std::vector<int>{1, 1, -2};
    MSSshare_add_res_preprocess_multi(party_id, &bprime, s1_vec, coeff1_vec);

    MSSshare dif(ell);
    auto s2_vec = make_ptr_vec(x, y);
    auto coeff2_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &dif, s2_vec, coeff2_vec);

    // Step 5: 预处理 bprime * dif
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &bprime_mul_dif, &bprime, &dif);

    // Step 6: 预处理最终结果 z = bprime * dif + y
    auto s3_vec = make_ptr_vec<MSSshare>(bprime_mul_dif, y);
    auto coeff3_vec = std::vector<int>{1, 1};
    MSSshare_add_res_preprocess_multi(party_id, &z, s3_vec, coeff3_vec);
}

void PI_select(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               PI_select_intermediate &intermediate, MSSshare *input_x, MSSshare *input_y,
               MSSshare *input_b, MSSshare *output_z) {
    int ell = input_x->BITLEN;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_select: input_x must be shared before calling PI_select");
    }
    if (!input_y->has_shared) {
        error("PI_select: input_y must be shared before calling PI_select");
    }
    if (!input_b->has_shared) {
        error("PI_select: input_b must be shared before calling PI_select");
    }
    if (!intermediate.has_preprocess) {
        error("PI_select: intermediate must be preprocessed before calling PI_select");
    }
    if (!output_z->has_preprocess) {
        error("PI_select: output_z must be preprocessed before calling PI_select");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell || output_z->BITLEN != ell) {
        error("PI_select: input bit-length mismatch");
    }
    if (input_b->BITLEN != 1) {
        error("PI_select: input_b must be a 1-bit share");
    }
#endif

    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare &b = *input_b;
    MSSshare &z = *output_z;
    MSSshare &rb = intermediate.rb;
    MSSshare_mul_res &mb_mul_rb = intermediate.mb_mul_rb;
    MSSshare_mul_res &bprime_mul_dif = intermediate.bprime_mul_dif;

    // Step 1: 设置 mb
    MSSshare mb(ell);
#ifdef DEBUG_MODE
    mb.has_preprocess = true;
    mb.has_shared = true;
#endif
    if (party_id == 1 || party_id == 2) {
        mb.v1 = b.v1;
    }

    // Step 2: 计算 mb*rb
    MSSshare_mul_res_calc_mul(party_id, netio, &mb_mul_rb, &mb, &rb);

    // Step 3: 计算 bprime = mb + rb -2*mb*rb, dif = x - y
    MSSshare bprime(ell);
    auto s1_vec = make_ptr_vec(mb, rb, mb_mul_rb);
    auto coeff1_vec = std::vector<int>{1, 1, -2};
    MSSshare_add_res_preprocess_multi(party_id, &bprime, s1_vec, coeff1_vec);
    MSSshare_add_res_calc_add_multi(party_id, &bprime, s1_vec, coeff1_vec);

    MSSshare dif(ell);
    auto s2_vec = make_ptr_vec(x, y);
    auto coeff2_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &dif, s2_vec, coeff2_vec);
    MSSshare_add_res_calc_add_multi(party_id, &dif, s2_vec, coeff2_vec);

    // Step 4: 计算 bprime * dif
    MSSshare_mul_res_calc_mul(party_id, netio, &bprime_mul_dif, &bprime, &dif);

    // Step 5: 计算最终结果 z = bprime * dif + y
    auto s3_vec = make_ptr_vec<MSSshare>(bprime_mul_dif, y);
    auto coeff3_vec = std::vector<int>{1, 1};
    MSSshare_add_res_calc_add_multi(party_id, &z, s3_vec, coeff3_vec);
}

void PI_select_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   std::vector<PI_select_intermediate *> &intermediate_vec,
                   std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare *> &input_y_vec,
                   std::vector<MSSshare *> &input_b_vec, std::vector<MSSshare *> &output_z_vec) {
#ifdef DEBUG_MODE
    if (input_x_vec.size() != intermediate_vec.size() ||
        input_y_vec.size() != intermediate_vec.size() ||
        input_b_vec.size() != intermediate_vec.size() ||
        output_z_vec.size() != intermediate_vec.size()) {
        error("PI_select_vec: input vector sizes do not match");
    }
    for (size_t i = 0; i < input_x_vec.size(); i++) {
        MSSshare *input_x = input_x_vec[i];
        MSSshare *input_y = input_y_vec[i];
        MSSshare *input_b = input_b_vec[i];
        MSSshare *output_z = output_z_vec[i];
        PI_select_intermediate &intermediate = *intermediate_vec[i];
        int ell = intermediate.ell;
        if (!input_x->has_shared) {
            error("PI_select_vec: input_x must be shared before calling PI_select_vec");
        }
        if (!input_y->has_shared) {
            error("PI_select_vec: input_y must be shared before calling PI_select_vec");
        }
        if (!input_b->has_shared) {
            error("PI_select_vec: input_b must be shared before calling PI_select_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_select_vec: intermediate must be preprocessed before calling PI_select_vec");
        }
        if (!output_z->has_preprocess) {
            error("PI_select_vec: output_z must be preprocessed before calling PI_select_vec");
        }
        if (input_x->BITLEN != ell || input_y->BITLEN != ell || output_z->BITLEN != ell) {
            error("PI_select_vec: input bit-length mismatch");
        }
        if (input_b->BITLEN != 1) {
            error("PI_select_vec: input_b must be a 1-bit share");
        }
    }
#endif

    int vec_size = input_x_vec.size();
    // for (int i = 0; i < vec_size; i++) {
    //     int ell = intermediate_vec[i]->ell;
    //     MSSshare &x = *input_x_vec[i];
    //     MSSshare &y = *input_y_vec[i];
    //     MSSshare &b = *input_b_vec[i];
    //     MSSshare &z = *output_z_vec[i];
    //     MSSshare &rb = intermediate_vec[i]->rb;
    //     MSSshare_mul_res &mb_mul_rb = intermediate_vec[i]->mb_mul_rb;
    //     MSSshare_mul_res &bprime_mul_dif = intermediate_vec[i]->bprime_mul_dif;
    // }

    // Step 1: 设置 mb
    std::vector<MSSshare> mb_vec;
    for (int i = 0; i < vec_size; i++) {
        int ell = intermediate_vec[i]->ell;
        MSSshare &b = *input_b_vec[i];

        MSSshare mb(ell);
#ifdef DEBUG_MODE
        mb.has_preprocess = true;
        mb.has_shared = true;
#endif
        if (party_id == 1 || party_id == 2) {
            mb.v1 = b.v1;
        }
        mb_vec.push_back(mb);
    }

    // Step 2: 计算 mb*rb
    {
        std::vector<MSSshare_mul_res *> mb_mul_rb_vec_ptr;
        std::vector<MSSshare *> rb_vec_ptr;
        std::vector<MSSshare *> mb_vec_ptr = make_ptr_vec(mb_vec);
        for (int i = 0; i < vec_size; i++) {
            mb_mul_rb_vec_ptr.push_back(&intermediate_vec[i]->mb_mul_rb);
            rb_vec_ptr.push_back(&intermediate_vec[i]->rb);
        }
        MSSshare_mul_res_calc_mul_vec(party_id, netio, mb_mul_rb_vec_ptr, mb_vec_ptr, rb_vec_ptr);
    }

    // Step 3: 计算 bprime = mb + rb -2*mb*rb, dif = x - y
    std::vector<MSSshare> bprime_vec;
    std::vector<MSSshare> dif_vec;
    for (int i = 0; i < vec_size; i++) {
        int ell = intermediate_vec[i]->ell;
        MSSshare &x = *input_x_vec[i];
        MSSshare &y = *input_y_vec[i];
        MSSshare &b = *input_b_vec[i];
        MSSshare &z = *output_z_vec[i];
        MSSshare &rb = intermediate_vec[i]->rb;
        MSSshare_mul_res &mb_mul_rb = intermediate_vec[i]->mb_mul_rb;
        MSSshare_mul_res &bprime_mul_dif = intermediate_vec[i]->bprime_mul_dif;

        MSSshare bprime(ell);
        auto s1_vec = make_ptr_vec(mb_vec[i], rb, mb_mul_rb);
        auto coeff1_vec = std::vector<int>{1, 1, -2};
        MSSshare_add_res_preprocess_multi(party_id, &bprime, s1_vec, coeff1_vec);
        MSSshare_add_res_calc_add_multi(party_id, &bprime, s1_vec, coeff1_vec);

        MSSshare dif(ell);
        auto s2_vec = make_ptr_vec(x, y);
        auto coeff2_vec = std::vector<int>{1, -1};
        MSSshare_add_res_preprocess_multi(party_id, &dif, s2_vec, coeff2_vec);
        MSSshare_add_res_calc_add_multi(party_id, &dif, s2_vec, coeff2_vec);

        bprime_vec.push_back(bprime);
        dif_vec.push_back(dif);
    }

    // Step 4: 计算 bprime * dif
    {
        std::vector<MSSshare_mul_res *> bprime_mul_dif_vec_ptr;
        std::vector<MSSshare *> dif_vec_ptr = make_ptr_vec(dif_vec);
        std::vector<MSSshare *> bprime_vec_ptr = make_ptr_vec(bprime_vec);
        for (int i = 0; i < vec_size; i++) {
            bprime_mul_dif_vec_ptr.push_back(&intermediate_vec[i]->bprime_mul_dif);
        }
        MSSshare_mul_res_calc_mul_vec(party_id, netio, bprime_mul_dif_vec_ptr, bprime_vec_ptr,
                                      dif_vec_ptr);
    }

    // Step 5: 计算最终结果 z = bprime * dif + y
    for (int i = 0; i < vec_size; i++) {
        int ell = intermediate_vec[i]->ell;
        MSSshare &x = *input_x_vec[i];
        MSSshare &y = *input_y_vec[i];
        MSSshare &b = *input_b_vec[i];
        MSSshare &z = *output_z_vec[i];
        MSSshare &rb = intermediate_vec[i]->rb;
        MSSshare_mul_res &mb_mul_rb = intermediate_vec[i]->mb_mul_rb;
        MSSshare_mul_res &bprime_mul_dif = intermediate_vec[i]->bprime_mul_dif;

        auto s3_vec = make_ptr_vec<MSSshare>(bprime_mul_dif, y);
        auto coeff3_vec = std::vector<int>{1, 1};
        MSSshare_add_res_calc_add_multi(party_id, &z, s3_vec, coeff3_vec);
    }
}