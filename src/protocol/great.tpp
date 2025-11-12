
void PI_great_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                         PI_great_intermediate &intermediate, MSSshare *input_x, MSSshare *input_y,
                         MSSshare_p *output_b) {
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
    intermediate.has_preprocess = true;
#endif

    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
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
#endif

    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
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