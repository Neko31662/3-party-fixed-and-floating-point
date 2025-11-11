template <int ell>
void PI_select_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                          PI_select_intermediate<ell> &intermediate, MSSshare<ell> *input_x,
                          MSSshare<ell> *input_y, MSSshare<1> *input_b,
                          MSSshare_add_res<ell> *output_z) {
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
    intermediate.has_preprocess = true;
#endif

    MSSshare<ell> &x = *input_x;
    MSSshare<ell> &y = *input_y;
    MSSshare<1> &b = *input_b;
    MSSshare_add_res<ell> &z = *output_z;
    MSSshare<ell> &rb = intermediate.rb;
    MSSshare_mul_res<ell> &mb_mul_rb = intermediate.mb_mul_rb;
    MSSshare_mul_res<ell> &bprime_mul_dif = intermediate.bprime_mul_dif;

    // Step 1: P0分享rb
    ShareValue rb_plain = (b.v1 + b.v2) & b.MASK;
    MSSshare_preprocess(0, party_id, PRGs, netio, &rb);
    MSSshare_share_from_store(party_id, netio, &rb, rb_plain);

    // Step 2: 生成MSSshare mb，r1=r2=0
    MSSshare<ell> mb;
#ifdef DEBUG_MODE
    mb.has_preprocess = true;
#endif

    // Step 3: 预处理 mb*rb
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &mb_mul_rb, &mb, &rb);

    // Step 4: 生成bprime = mb + rb -2*mb*rb, dif = x - y
    MSSshare_add_res<ell> bprime;
    auto s1_vec = make_ptr_vec(mb, rb, mb_mul_rb);
    auto coeff1_vec = std::vector<int>{1, 1, -2};
    MSSshare_add_res_preprocess_multi(party_id, &bprime, s1_vec, coeff1_vec);

    MSSshare_add_res<ell> dif;
    auto s2_vec = make_ptr_vec(x, y);
    auto coeff2_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &dif, s2_vec, coeff2_vec);

    // Step 5: 预处理 bprime * dif
    MSSshare_mul_res_preprocess(party_id, PRGs, netio, &bprime_mul_dif, &bprime, &dif);

    // Step 6: 预处理最终结果 z = bprime * dif + y
    auto s3_vec = make_ptr_vec<MSSshare<ell>>(bprime_mul_dif, y);
    auto coeff3_vec = std::vector<int>{1, 1};
    MSSshare_add_res_preprocess_multi(party_id, &z, s3_vec, coeff3_vec);
}

template <int ell>
void PI_select(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               PI_select_intermediate<ell> &intermediate, MSSshare<ell> *input_x,
               MSSshare<ell> *input_y, MSSshare<1> *input_b, MSSshare_add_res<ell> *output_z) {
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
#endif

    MSSshare<ell> &x = *input_x;
    MSSshare<ell> &y = *input_y;
    MSSshare<1> &b = *input_b;
    MSSshare_add_res<ell> &z = *output_z;
    MSSshare<ell> &rb = intermediate.rb;
    MSSshare_mul_res<ell> &mb_mul_rb = intermediate.mb_mul_rb;
    MSSshare_mul_res<ell> &bprime_mul_dif = intermediate.bprime_mul_dif;

    // Step 1: 设置 mb
    MSSshare<ell> mb;
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
    MSSshare_add_res<ell> bprime;
    auto s1_vec = make_ptr_vec(mb, rb, mb_mul_rb);
    auto coeff1_vec = std::vector<int>{1, 1, -2};
    MSSshare_add_res_preprocess_multi(party_id, &bprime, s1_vec, coeff1_vec);
    MSSshare_add_res_calc_add_multi(party_id, &bprime, s1_vec, coeff1_vec);

    MSSshare_add_res<ell> dif;
    auto s2_vec = make_ptr_vec(x, y);
    auto coeff2_vec = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &dif, s2_vec, coeff2_vec);
    MSSshare_add_res_calc_add_multi(party_id, &dif, s2_vec, coeff2_vec);

    // Step 4: 计算 bprime * dif
    MSSshare_mul_res_calc_mul(party_id, netio, &bprime_mul_dif, &bprime, &dif);

    // Step 5: 计算最终结果 z = bprime * dif + y
    auto s3_vec = make_ptr_vec<MSSshare<ell>>(bprime_mul_dif, y);
    auto coeff3_vec = std::vector<int>{1, 1};
    MSSshare_add_res_calc_add_multi(party_id, &z, s3_vec, coeff3_vec);
}