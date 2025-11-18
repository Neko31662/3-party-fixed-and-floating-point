void PI_sign_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                        PI_sign_intermediate &intermediate, MSSshare *input_x,
                        MSSshare_p *output_b) {
    int ell = intermediate.ell;
    ShareValue k = intermediate.k;
#ifdef DEBUG_MODE
    if (!input_x->has_preprocess) {
        error("PI_sign_preprocess: input_x must be preprocessed before calling PI_sign_preprocess");
    }
    if (intermediate.has_preprocess) {
        error("PI_sign_preprocess has already been called on this object");
    }
    if (output_b->has_preprocess) {
        error("PI_sign_preprocess: output_b has already been preprocessed");
    }
    if (output_b->p != k) {
        error("PI_sign_preprocess: output_b modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_sign_preprocess: input_x bitlen mismatch");
    }
    intermediate.has_preprocess = true;
#endif

    MSSshare &x = *input_x;
    MSSshare_p &b = *output_b;
    MSSshare_p &r_prime = intermediate.r_prime;
    bool &Delta = intermediate.Delta;
    ShareValue &Gamma = intermediate.Gamma;
    std::vector<ShareValue> &rx_list = intermediate.rx_list;
    ShareValue p = intermediate.p;
    int bytep = byte_of(p);
    int bytek = byte_of(k);

    // Step 1: 预处理r_prime和b
    MSSshare_p_preprocess(0, party_id, PRGs, netio, &r_prime);
    MSSshare_p_preprocess(0, party_id, PRGs, netio, &b);

    // Step 2: P0生成rx_list并分享
    if (party_id == 0 || party_id == 1) {
        // 生成rx_list的前ell-1位
        for (int i = 0; i < ell - 1; i++) {
            ShareValue r_i;
            PRGs[0].gen_random_data(&r_i, sizeof(ShareValue));
            r_i %= p;
            rx_list[i] = r_i;
        }
    }

    if (party_id == 0) {
        ShareValue rx_hat = (x.v1 + x.v2) & (x.MASK >> 1);
        // 第0个值是最高位，第ell-2个值是最低位
        std::vector<ShareValue> rx_hat_bits(ell - 1);
        for (int i = 0; i < ell - 1; i++) {
            rx_hat_bits[i] = (rx_hat >> (ell - 2 - i)) & 1;
            rx_hat_bits[i] = rx_hat_bits[i] ^ 1; // 取反
            rx_hat_bits[i] = (rx_hat_bits[i] - rx_list[i] + p) % p;
            netio.store_data(2, &rx_hat_bits[i], bytep);
        }
        // need to send later
    } else if (party_id == 2) {
        for (int i = 0; i < ell - 1; i++) {
            ShareValue r_i = 0;
            netio.recv_data(0, &r_i, bytep);
            rx_list[i] = r_i;
        }
    }

    // Step 3: P1和P2生成Delta并计算Gamma
    if (party_id == 1 || party_id == 2) {
        ShareValue tmp = 0;
        PRGs[1].gen_random_data(&tmp, sizeof(ShareValue));
        Delta = tmp & 1;
        Gamma = ((Delta ? -r_prime.v2 : r_prime.v2) - b.v2 + (k << 2)) % k;
        if (party_id == 1) {
            Gamma = (Gamma + Delta) % k;
        }
        // 预处理期间，只有P1向P2发送Gamma，以防写法上通信轮数过高，实际上P2也可以在预处理期间向P1发送
        if (party_id == 1) {
            netio.store_data(2, &Gamma, bytek);
            // need to send later
        } else if (party_id == 2) {
            tmp = 0;
            netio.recv_data(1, &tmp, bytek);
            Gamma = (Gamma + tmp) % k;
        }
    }
}

void PI_sign(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
             PI_sign_intermediate &intermediate, MSSshare *input_x, MSSshare_p *output_b) {
    int ell = intermediate.ell;
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_sign: input_x must be shared before calling PI_sign");
    }
    if (!intermediate.has_preprocess) {
        error("PI_sign: intermediate must be preprocessed before calling PI_sign");
    }
    if (!output_b->has_preprocess) {
        error("PI_sign: output_b must be preprocessed before calling PI_sign");
    }
    if (output_b->p != intermediate.k) {
        error("PI_sign: output_b modulus mismatch");
    }
    if (input_x->BITLEN != ell) {
        error("PI_sign: input_x bitlen mismatch");
    }
    output_b->has_shared = true;
#endif

    ShareValue k = intermediate.k;
    MSSshare &x = *input_x;
    MSSshare_p &b = *output_b;
    MSSshare_p &r_prime = intermediate.r_prime;
    bool &Delta = intermediate.Delta;
    ShareValue &Gamma = intermediate.Gamma;
    std::vector<ShareValue> &rx_list = intermediate.rx_list;
    ShareValue p = intermediate.p;
    int bytep = byte_of(p);
    int bytek = byte_of(k);

    // Step 0: P2将Gamma发送给P1
    if (party_id == 2) {
        netio.send_data(1, &Gamma, bytek);
    } else if (party_id == 1) {
        Gamma = 0;
        netio.recv_data(2, &Gamma, bytek);
    }

    if (party_id == 1 || party_id == 2) {
        // Step 1: 计算mx_list，将rx_list和mx_list最后一位设为0
        std::vector<bool> mx_list(ell);
        ShareValue mx_hat = (x.v1) & (x.MASK >> 1);
        // 第0个值是最高位，第ell-2个值是最低位
        for (int i = 0; i < ell - 1; i++) {
            mx_list[i] = (mx_hat >> (ell - 2 - i)) & 1;
        }
        mx_list[ell - 1] = 0;
        rx_list[ell - 1] = party_id - 1;

        // Step 2: 计算xor_list
        std::vector<ShareValue> xor_list(ell);
        for (int i = 0; i < ell; i++) {
            xor_list[i] = (rx_list[i] - 2 * mx_list[i] * rx_list[i] + 2 * p * p) % p;
            if (party_id == 1) {
                xor_list[i] = (xor_list[i] + mx_list[i]) % p;
            }
        }

        // Step3: 计算pre_list
        std::vector<ShareValue> pre_list(ell);
        ShareValue sum = 0;
        for (int i = 0; i < ell; i++) {
            sum = (sum + xor_list[i]) % p;
            pre_list[i] = (sum - 2 * xor_list[i] + 2 * p) % p;
            if (party_id == 1) {
                pre_list[i] = (pre_list[i] + 1) % p;
            }
        }

        // Step4: 计算encode_list
        bool mx_sign = ((x.v1 & x.MASK) >> (ell - 1));
        std::vector<ShareValue> encode_list(ell);
        std::vector<ShareValue> w_list(ell);
        for (int i = 0; i < ell; i++) {
            PRGs[1].gen_random_data(&w_list[i], sizeof(ShareValue));
            w_list[i] = (w_list[i] % (p - 1)) + 1; // 确保w在1~p-1之间
        }
        for (int i = 0; i < ell; i++) {
            ShareValue tmp;
            if (mx_sign ^ Delta ^ mx_list[i] == 1) {
                tmp = pre_list[i];
            } else {
                tmp = party_id - 1;
            }
            encode_list[i] = w_list[i] * tmp % p;
        }

        auto pi = gen_random_permutation(ell, PRGs[1]);
        apply_permutation(pi, encode_list.data());

        // Step 5:把结果发送给P0
        for (int i = 0; i < ell; i++) {
            netio.store_data(0, &encode_list[i], bytep);
        }
        netio.send_stored_data(0);
    }

    if (party_id == 0) {
        // Step 6: P0接收并解码encode_list
        std::vector<ShareValue> encode_list(ell);
        for (int i = 0; i < ell; i++) {
            encode_list[i] = 0;
            netio.recv_data(1, &encode_list[i], bytep);
        }
        for (int i = 0; i < ell; i++) {
            ShareValue val = 0;
            netio.recv_data(2, &val, bytep);
            encode_list[i] = (encode_list[i] + val) % p;
        }

        // Step 7:检测是否存在0，并计算masked_t
        int cnt = 0;
        for (int i = 0; i < ell; i++) {
            if (encode_list[i] == 0) {
                cnt++;
            }
        }
        if (cnt > 1) {
            error("PI_sign: P0 find more than one zeros in encode_list");
        }
        ShareValue r = (x.v1 + x.v2) & x.MASK;
        bool t = r >> (ell - 1);
        if (cnt) {
            t ^= 1;
        }
        ShareValue masked_t = (t - r_prime.v1 - r_prime.v2 + 2 * k) % k;

        // Step 8: 发送masked_t给P1和P2
        netio.send_data(1, &masked_t, bytek);
        netio.send_data(2, &masked_t, bytek);
    }

    if (party_id == 1 || party_id == 2) {
        // Step 9: 接收masked_t并计算最终结果b
        ShareValue masked_t = 0;
        netio.recv_data(0, &masked_t, bytek);
        if (Delta) {
            masked_t = k - masked_t;
        }
        b.v1 = (masked_t + Gamma) % k;
    }
}

void PI_sign_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 std::vector<PI_sign_intermediate *> &intermediate_vec,
                 std::vector<MSSshare *> input_x_vec, std::vector<MSSshare_p *> output_b_vec) {
#ifdef DEBUG_MODE
    if (input_x_vec.size() != intermediate_vec.size() ||
        output_b_vec.size() != intermediate_vec.size()) {
        error("PI_sign_vec: input vector sizes do not match");
    }
    for (size_t i = 0; i < input_x_vec.size(); i++) {
        MSSshare *input_x = input_x_vec[i];
        PI_sign_intermediate &intermediate = *intermediate_vec[i];
        MSSshare_p *output_b = output_b_vec[i];
        int ell = intermediate.ell;
        if (!input_x->has_shared) {
            error("PI_sign_vec: input_x must be shared before calling PI_sign");
        }
        if (!intermediate.has_preprocess) {
            error("PI_sign_vec: intermediate must be preprocessed before calling PI_sign");
        }
        if (!output_b->has_preprocess) {
            error("PI_sign_vec: output_b must be preprocessed before calling PI_sign");
        }
        if (output_b->p != intermediate.k) {
            error("PI_sign_vec: output_b modulus mismatch");
        }
        if (input_x->BITLEN != ell) {
            error("PI_sign_vec: input_x bitlen mismatch");
        }
        output_b->has_shared = true;
    }
#endif
    const int vec_size = input_x_vec.size();

    ShareValue p[vec_size];
    int ell[vec_size];
    ShareValue k[vec_size];
    int bytep[vec_size];
    int bytek[vec_size];
    for (int i = 0; i < vec_size; i++) {
        p[i] = intermediate_vec[i]->p;
        ell[i] = intermediate_vec[i]->ell;
        k[i] = intermediate_vec[i]->k;
        bytep[i] = byte_of(p[i]);
        bytek[i] = byte_of(k[i]);
    }

    // Step 0: P2将Gamma发送给P1
    for (size_t i = 0; i < vec_size; i++) {
        ShareValue &Gamma = intermediate_vec[i]->Gamma;

        if (party_id == 2) {
            netio.store_data(1, (char *)&Gamma, bytek[i]);
        } else if (party_id == 1) {
            Gamma = 0;
            netio.recv_data(2, (char *)&Gamma, bytek[i]);
        }
    }
    if (party_id == 2) {
        netio.send_stored_data(1);
    }

    if (party_id == 1 || party_id == 2) {
        for (size_t idx = 0; idx < vec_size; idx++) {
            MSSshare *input_x = input_x_vec[idx];
            PI_sign_intermediate &intermediate = *intermediate_vec[idx];
            MSSshare_p *output_b = output_b_vec[idx];

            MSSshare &x = *input_x;
            MSSshare_p &b = *output_b;
            MSSshare_p &r_prime = intermediate.r_prime;
            bool &Delta = intermediate.Delta;
            ShareValue &Gamma = intermediate.Gamma;
            std::vector<ShareValue> &rx_list = intermediate.rx_list;

            // Step 1: 计算mx_list，将rx_list和mx_list最后一位设为0
            std::vector<bool> mx_list(ell[idx]);
            ShareValue mx_hat = (x.v1) & (x.MASK >> 1);
            // 第0个值是最高位，第ell-2个值是最低位
            for (int i = 0; i < ell[idx] - 1; i++) {
                mx_list[i] = (mx_hat >> (ell[idx] - 2 - i)) & 1;
            }
            mx_list[ell[idx] - 1] = 0;
            rx_list[ell[idx] - 1] = party_id - 1;

            // Step 2: 计算xor_list
            std::vector<ShareValue> xor_list(ell[idx]);
            for (int i = 0; i < ell[idx]; i++) {
                xor_list[i] =
                    (rx_list[i] - 2 * mx_list[i] * rx_list[i] + 2 * p[idx] * p[idx]) % p[idx];
                if (party_id == 1) {
                    xor_list[i] = (xor_list[i] + mx_list[i]) % p[idx];
                }
            }

            // Step3: 计算pre_list
            std::vector<ShareValue> pre_list(ell[idx]);
            ShareValue sum = 0;
            for (int i = 0; i < ell[idx]; i++) {
                sum = (sum + xor_list[i]) % p[idx];
                pre_list[i] = (sum - 2 * xor_list[i] + 2 * p[idx]) % p[idx];
                if (party_id == 1) {
                    pre_list[i] = (pre_list[i] + 1) % p[idx];
                }
            }

            // Step4: 计算encode_list
            bool mx_sign = ((x.v1 & x.MASK) >> (ell[idx] - 1));
            std::vector<ShareValue> encode_list(ell[idx]);
            std::vector<ShareValue> w_list(ell[idx]);
            for (int i = 0; i < ell[idx]; i++) {
                PRGs[1].gen_random_data(&w_list[i], sizeof(ShareValue));
                w_list[i] = (w_list[i] % (p[idx] - 1)) + 1; // 确保w在1~p[idx]-1之间
            }
            for (int i = 0; i < ell[idx]; i++) {
                ShareValue tmp;
                if (mx_sign ^ Delta ^ mx_list[i] == 1) {
                    tmp = pre_list[i];
                } else {
                    tmp = party_id - 1;
                }
                encode_list[i] = w_list[i] * tmp % p[idx];
            }

            auto pi = gen_random_permutation(ell[idx], PRGs[1]);
            apply_permutation(pi, encode_list.data());

            // Step 5:把结果发送给P0
            for (int i = 0; i < ell[idx]; i++) {
                netio.store_data(0, (char *)&encode_list[i], bytep[idx]);
            }
        }
        netio.send_stored_data(0);
    }

    if (party_id == 0) {
        // Step 6: P0接收并解码encode_list

        for (size_t idx = 0; idx < vec_size; idx++) {

            MSSshare *input_x = input_x_vec[idx];
            PI_sign_intermediate &intermediate = *intermediate_vec[idx];
            MSSshare_p *output_b = output_b_vec[idx];

            MSSshare &x = *input_x;
            MSSshare_p &b = *output_b;
            MSSshare_p &r_prime = intermediate.r_prime;
            bool &Delta = intermediate.Delta;

            std::vector<ShareValue> encode_list(ell[idx]);
            for (int i = 0; i < ell[idx]; i++) {
                encode_list[i] = 0;
                netio.recv_data(1, (char *)&encode_list[i], bytep[idx]);
            }
            for (int i = 0; i < ell[idx]; i++) {
                ShareValue val = 0;
                netio.recv_data(2, (char *)&val, bytep[idx]);
                encode_list[i] = (encode_list[i] + val) % p[idx];
            }

            // Step 7:检测是否存在0，并计算masked_t
            int cnt = 0;
            for (int i = 0; i < ell[idx]; i++) {
                if (encode_list[i] == 0) {
                    cnt++;
                }
            }
            if (cnt > 1) {
                error("PI_sign_vec: P0 find more than one zeros in encode_list");
            }
            ShareValue r = (x.v1 + x.v2) & x.MASK;
            bool t = r >> (ell[idx] - 1);
            if (cnt) {
                t ^= 1;
            }
            ShareValue masked_t = (t - r_prime.v1 - r_prime.v2 + 2 * k[idx]) % k[idx];

            // Step 8: 发送masked_t给P1和P2
            netio.store_data(1, (char *)&masked_t, bytek[idx]);
            netio.store_data(2, (char *)&masked_t, bytek[idx]);
        }
        netio.send_stored_data(1);
        netio.send_stored_data(2);
    }

    if (party_id == 1 || party_id == 2) {
        for (size_t idx = 0; idx < vec_size; idx++) {
            PI_sign_intermediate &intermediate = *intermediate_vec[idx];
            MSSshare_p *output_b = output_b_vec[idx];
            MSSshare_p &b = *output_b;
            bool &Delta = intermediate.Delta;
            ShareValue &Gamma = intermediate.Gamma;

            // Step 9: 接收masked_t并计算最终结果b
            ShareValue masked_t = 0;
            netio.recv_data(0, (char *)&masked_t, bytek[idx]);
            if (Delta) {
                masked_t = k[idx] - masked_t;
            }
            b.v1 = (masked_t + Gamma) % k[idx];
        }
    }
}