
void PI_great_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_great_intermediate &intermediate, MSSshare *input_x, MSSshare *input_y,
                         MSSshare_p *output_b) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_preprocess) {
        error(
            "PI_great_preprocess: input_x must be preprocessed before calling PI_great_preprocess");
    }
    if (!input_y->has_preprocess) {
        error(
            "PI_great_preprocess: input_y must be preprocessed before calling PI_great_preprocess");
    }
    if (intermediate.has_preprocess) {
        error("PI_great_preprocess has already been called on this object");
    }
    if (output_b->has_preprocess) {
        error("PI_great_preprocess: output_b has already been preprocessed");
    }
    if (output_b->p != intermediate.k) {
        error("PI_great_preprocess: output_b modulus mismatch");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
        error("PI_great_preprocess: input_x or input_y bitlen mismatch");
    }
    intermediate.has_preprocess = true;
#endif

    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare_p &b = *output_b;
    std::vector<PI_sign_intermediate> &sign_intermediate = intermediate.sign_intermediate;
    std::vector<MSSshare_p> &sign_res = intermediate.sign_res;
    MSSshare_p_mul_res &tmp1 = intermediate.tmp1;
    MSSshare_p_mul_res &tmp2 = intermediate.tmp2;

    // y - x
    MSSshare tmp_add1(ell);
    auto add_vec1 = make_ptr_vec(y, x);
    auto coeff_vec1 = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &tmp_add1, add_vec1, coeff_vec1);

    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate[0], &x, &sign_res[0]);        // sx
    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate[1], &y, &sign_res[1]);        // sy
    PI_sign_preprocess(party_id, PRGs, netio, sign_intermediate[2], &tmp_add1, &sign_res[2]); // sd

    // sx * sy
    MSSshare_p_mul_res_preprocess(party_id, PRGs, netio, &tmp1, &sign_res[0], &sign_res[1]);

    // 1 - sx - sy + 2sx*sy
    MSSshare_p tmp_add2{k};
    auto add_vec2 = make_ptr_vec(sign_res[0], sign_res[1], tmp1);
    auto coeff_vec2 = std::vector<int>{-1, -1, 2};
    MSSshare_p_add_res_preprocess_multi(party_id, &tmp_add2, add_vec2, coeff_vec2);

    // (1 - sx - sy + 2sx*sy) * sd
    MSSshare_p_mul_res_preprocess(party_id, PRGs, netio, &tmp2, &tmp_add2, &sign_res[2]);

    // b = (1 - sx - sy + 2sx*sy) * sd - sx*sy + sy
    auto final_add_vec = make_ptr_vec<MSSshare_p>(tmp2, tmp1, sign_res[1]);
    auto final_coeff_vec = std::vector<int>{1, -1, 1};
    MSSshare_p_add_res_preprocess_multi(party_id, &b, final_add_vec, final_coeff_vec);
}

void PI_great(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
              PI_great_intermediate &intermediate, MSSshare *input_x, MSSshare *input_y,
              MSSshare_p *output_b) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_great: input_x must be shared before calling PI_great");
    }
    if (!input_y->has_shared) {
        error("PI_great: input_y must be shared before calling PI_great");
    }
    if (!intermediate.has_preprocess) {
        error("PI_great: intermediate must be preprocessed before calling PI_great");
    }
    if (!output_b->has_preprocess) {
        error("PI_great: output_b must be preprocessed before calling PI_great");
    }
    if (output_b->p != intermediate.k) {
        error("PI_great: output_b modulus mismatch");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
        error("PI_great: input_x or input_y bitlen mismatch");
    }
#endif

    MSSshare &x = *input_x;
    MSSshare &y = *input_y;
    MSSshare_p &b = *output_b;
    std::vector<PI_sign_intermediate> &sign_intermediate = intermediate.sign_intermediate;
    std::vector<MSSshare_p> &sign_res = intermediate.sign_res;
    MSSshare_p_mul_res &tmp1 = intermediate.tmp1;
    MSSshare_p_mul_res &tmp2 = intermediate.tmp2;

    // y - x
    MSSshare tmp_add1(ell);
    auto add_vec1 = make_ptr_vec(y, x);
    auto coeff_vec1 = std::vector<int>{1, -1};
    MSSshare_add_res_preprocess_multi(party_id, &tmp_add1, add_vec1, coeff_vec1);
    MSSshare_add_res_calc_add_multi(party_id, &tmp_add1, add_vec1, coeff_vec1);

    auto sign_intermediate_vec = make_ptr_vec(sign_intermediate);
    auto sign_input_vec = make_ptr_vec(x, y, tmp_add1);
    auto sign_res_vec = make_ptr_vec(sign_res);

    PI_sign_vec(party_id, PRGs, netio, sign_intermediate_vec, sign_input_vec, sign_res_vec);

    // sx * sy
    MSSshare_p_mul_res_calc_mul(party_id, netio, &tmp1, &sign_res[0], &sign_res[1]);

    // 1 - sx - sy + 2sx*sy
    MSSshare_p tmp_add2{k};
    auto add_vec2 = make_ptr_vec(sign_res[0], sign_res[1], tmp1);
    auto coeff_vec2 = std::vector<int>{-1, -1, 2};
    MSSshare_p_add_res_preprocess_multi(party_id, &tmp_add2, add_vec2, coeff_vec2);
    MSSshare_p_add_res_calc_add_multi(party_id, &tmp_add2, add_vec2, coeff_vec2);
    if (party_id == 1 || party_id == 2) {
        tmp_add2.v1 = (tmp_add2.v1 + 1) % tmp_add2.p;
    }

    // (1 - sx - sy + 2sx*sy) * sd
    MSSshare_p_mul_res_calc_mul(party_id, netio, &tmp2, &tmp_add2, &sign_res[2]);

    // b = (1 - sx - sy + 2sx*sy) * sd - sx*sy + sy
    auto final_add_vec = make_ptr_vec<MSSshare_p>(tmp2, tmp1, sign_res[1]);
    auto final_coeff_vec = std::vector<int>{1, -1, 1};
    MSSshare_p_add_res_calc_add_multi(party_id, &b, final_add_vec, final_coeff_vec);
}

void PI_great_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  std::vector<PI_great_intermediate *> &intermediate_vec,
                  std::vector<MSSshare *> &input_x_vec, std::vector<MSSshare *> &input_y_vec,
                  std::vector<MSSshare_p *> &output_b_vec) {
#ifdef DEBUG_MODE
    if (intermediate_vec.size() != input_x_vec.size() ||
        intermediate_vec.size() != input_y_vec.size() ||
        intermediate_vec.size() != output_b_vec.size()) {
        error("PI_great_vec: vector size mismatch");
    }
    for (size_t i = 0; i < intermediate_vec.size(); i++) {
        PI_great_intermediate &intermediate = *intermediate_vec[i];
        MSSshare *input_x = input_x_vec[i];
        MSSshare *input_y = input_y_vec[i];
        MSSshare_p *output_b = output_b_vec[i];
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;
        if (!input_x->has_shared) {
            error("PI_great_vec: input_x must be shared before calling PI_great_vec");
        }
        if (!input_y->has_shared) {
            error("PI_great_vec: input_y must be shared before calling PI_great_vec");
        }
        if (!intermediate.has_preprocess) {
            error("PI_great_vec: intermediate must be preprocessed before calling PI_great_vec");
        }
        if (!output_b->has_preprocess) {
            error("PI_great_vec: output_b must be preprocessed before calling PI_great_vec");
        }
        if (output_b->p != intermediate.k) {
            error("PI_great_vec: output_b modulus mismatch");
        }
        if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
            error("PI_great_vec: input_x or input_y bitlen mismatch");
        }
    }
#endif

    int vec_size = intermediate_vec.size();
    std::vector<MSSshare> tmp_add1_vec;
    tmp_add1_vec.reserve(vec_size); // 预留空间以避免扩容导致的指针失效
    std::vector<PI_sign_intermediate *> sign_intermediate_vec;
    std::vector<MSSshare *> sign_input_vec;
    std::vector<MSSshare_p *> sign_res_vec;
    for (int i = 0; i < vec_size; i++) {
        MSSshare &x = *input_x_vec[i];
        MSSshare &y = *input_y_vec[i];
        MSSshare_p &b = *output_b_vec[i];
        PI_great_intermediate &intermediate = *intermediate_vec[i];
        std::vector<PI_sign_intermediate> &sign_intermediate = intermediate.sign_intermediate;
        std::vector<MSSshare_p> &sign_res = intermediate.sign_res;
        MSSshare_p_mul_res &tmp1 = intermediate.tmp1;
        MSSshare_p_mul_res &tmp2 = intermediate.tmp2;
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;

        // y - x
        MSSshare tmp_add1(ell);
        auto add_vec1 = make_ptr_vec(y, x);
        auto coeff_vec1 = std::vector<int>{1, -1};
        MSSshare_add_res_preprocess_multi(party_id, &tmp_add1, add_vec1, coeff_vec1);
        MSSshare_add_res_calc_add_multi(party_id, &tmp_add1, add_vec1, coeff_vec1);
        tmp_add1_vec.push_back(tmp_add1);

        std::vector<PI_sign_intermediate *> sign_intermediate_vec_tmp =
            make_ptr_vec(sign_intermediate);
        std::vector<MSSshare *> sign_input_vec_tmp = make_ptr_vec(x, y, tmp_add1_vec.back());
        std::vector<MSSshare_p *> sign_res_vec_tmp = make_ptr_vec(sign_res);
        for (auto &ptr : sign_intermediate_vec_tmp) {
            sign_intermediate_vec.push_back(ptr);
        }
        for (auto &ptr : sign_input_vec_tmp) {
            sign_input_vec.push_back(ptr);
        }
        for (auto &ptr : sign_res_vec_tmp) {
            sign_res_vec.push_back(ptr);
        }
    }

    PI_sign_vec(party_id, PRGs, netio, sign_intermediate_vec, sign_input_vec, sign_res_vec);

    // sx * sy
    {
        std::vector<MSSshare_p_mul_res *> tmp1_ptrs;
        std::vector<MSSshare_p *> sign_res0_ptrs;
        std::vector<MSSshare_p *> sign_res1_ptrs;
        for (int i = 0; i < vec_size; i++) {
            PI_great_intermediate &intermediate = *intermediate_vec[i];
            std::vector<MSSshare_p> &sign_res = intermediate.sign_res;
            MSSshare_p_mul_res &tmp1 = intermediate.tmp1;
            tmp1_ptrs.push_back(&tmp1);
            sign_res0_ptrs.push_back(&sign_res[0]);
            sign_res1_ptrs.push_back(&sign_res[1]);
        }
        MSSshare_p_mul_res_calc_mul_vec(party_id, netio, tmp1_ptrs, sign_res0_ptrs, sign_res1_ptrs);
    }

    // 1 - sx - sy + 2sx*sy
    std::vector<MSSshare_p> tmp_add2_vec;
    for (int i = 0; i < vec_size; i++) {
        MSSshare &x = *input_x_vec[i];
        MSSshare &y = *input_y_vec[i];
        MSSshare_p &b = *output_b_vec[i];
        PI_great_intermediate &intermediate = *intermediate_vec[i];
        std::vector<PI_sign_intermediate> &sign_intermediate = intermediate.sign_intermediate;
        std::vector<MSSshare_p> &sign_res = intermediate.sign_res;
        MSSshare_p_mul_res &tmp1 = intermediate.tmp1;
        MSSshare_p_mul_res &tmp2 = intermediate.tmp2;
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;

        MSSshare_p tmp_add2{k};
        auto add_vec2 = make_ptr_vec(sign_res[0], sign_res[1], tmp1);
        auto coeff_vec2 = std::vector<int>{-1, -1, 2};
        MSSshare_p_add_res_preprocess_multi(party_id, &tmp_add2, add_vec2, coeff_vec2);
        MSSshare_p_add_res_calc_add_multi(party_id, &tmp_add2, add_vec2, coeff_vec2);
        if (party_id == 1 || party_id == 2) {
            tmp_add2.v1 = (tmp_add2.v1 + 1) % tmp_add2.p;
        }
        tmp_add2_vec.push_back(tmp_add2);
    }

    // (1 - sx - sy + 2sx*sy) * sd
    {
        std::vector<MSSshare_p_mul_res *> tmp2_ptrs;
        std::vector<MSSshare_p *> tmp_add2_ptrs;
        std::vector<MSSshare_p *> sign_res2_ptrs;
        for (int i = 0; i < vec_size; i++) {
            PI_great_intermediate &intermediate = *intermediate_vec[i];
            std::vector<MSSshare_p> &sign_res = intermediate.sign_res;
            MSSshare_p_mul_res &tmp2 = intermediate.tmp2;
            tmp2_ptrs.push_back(&tmp2);
            tmp_add2_ptrs.push_back(&tmp_add2_vec[i]);
            sign_res2_ptrs.push_back(&sign_res[2]);
        }
        MSSshare_p_mul_res_calc_mul_vec(party_id, netio, tmp2_ptrs, tmp_add2_ptrs, sign_res2_ptrs);
    }

    // b = (1 - sx - sy + 2sx*sy) * sd - sx*sy + sy
    for (int i = 0; i < vec_size; i++) {
        MSSshare &x = *input_x_vec[i];
        MSSshare &y = *input_y_vec[i];
        MSSshare_p &b = *output_b_vec[i];
        PI_great_intermediate &intermediate = *intermediate_vec[i];
        std::vector<PI_sign_intermediate> &sign_intermediate = intermediate.sign_intermediate;
        std::vector<MSSshare_p> &sign_res = intermediate.sign_res;
        MSSshare_p_mul_res &tmp1 = intermediate.tmp1;
        MSSshare_p_mul_res &tmp2 = intermediate.tmp2;
        int ell = intermediate.ell;
        ShareValue k = intermediate.k;

        auto final_add_vec = make_ptr_vec<MSSshare_p>(tmp2, tmp1, sign_res[1]);
        auto final_coeff_vec = std::vector<int>{1, -1, 1};
        MSSshare_p_add_res_calc_add_multi(party_id, &b, final_add_vec, final_coeff_vec);
    }
}