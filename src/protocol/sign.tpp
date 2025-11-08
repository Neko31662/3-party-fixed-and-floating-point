template <int ell, ShareValue k>
void PI_sign_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                        PI_sign_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
                        MSSshare_p *output_b) {
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
    intermediate.has_preprocess = true;
#endif

    MSSshare<ell> &x = *input_x;
    MSSshare_p &b = *output_b;
    MSSshare_p &r_prime = intermediate.r_prime;
    bool &Delta = intermediate.Delta;
    ShareValue &Gamma = intermediate.Gamma;
    std::array<ShareValue, ell> &rx_list = intermediate.rx_list;
    ShareValue p = PI_sign_intermediate<ell, k>::p;

    ShareValue MASK_p = 1;
    int bitp = 1;
    while (MASK_p + 1 < p) {
        MASK_p = (MASK_p << 1) | 1;
        bitp++;
    }
    int bytep = (bitp + 7) / 8;

    ShareValue MASK_k = 1;
    int bitk = 1;
    while (MASK_k + 1 < k) {
        MASK_k = (MASK_k << 1) | 1;
        bitk++;
    }
    int bytek = (bitk + 7) / 8;

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
    encoded_msg rx_list_msg;
    if (party_id == 0) {
        ShareValue rx_hat = (x.v1 + x.v2) & (x.MASK >> 1);
        // 第0个值是最高位，第ell-2个值是最低位
        std::array<ShareValue, ell - 1> rx_hat_bits;
        for (int i = 0; i < ell - 1; i++) {
            rx_hat_bits[i] = (rx_hat >> (ell - 2 - i)) & 1;
            rx_hat_bits[i] = rx_hat_bits[i] ^ 1; // 取反
            rx_hat_bits[i] = (rx_hat_bits[i] - rx_list[i] + p) % p;
            rx_list_msg.add_msg((char *)&rx_hat_bits[i], bitp);
        }
        std::vector<char> tmp(rx_list_msg.get_bytelen());
        rx_list_msg.to_char_array(tmp.data());
        netio.store_data(2, tmp.data(), rx_list_msg.get_bytelen());
        // need to send later
    } else if (party_id == 2) {
        int bytelen = ((ell - 1) * bitp + 7) / 8;
        std::vector<char> tmp(bytelen);
        netio.recv_data(0, tmp.data(), bytelen);
        rx_list_msg.from_char_array(tmp.data(), bytelen);
        for (int i = 0; i < ell - 1; i++) {
            ShareValue r_i = 0;
            rx_list_msg.read_msg((char *)&r_i, bitp);
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

template <int ell, ShareValue k>
void PI_sign(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
             PI_sign_intermediate<ell, k> &intermediate, MSSshare<ell> *input_x,
             MSSshare_p *output_b) {
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
    if (output_b->p != k) {
        error("PI_sign_preprocess: output_b modulus mismatch");
    }
    output_b->has_shared = true;
#endif

    MSSshare<ell> &x = *input_x;
    MSSshare_p &b = *output_b;
    MSSshare_p &r_prime = intermediate.r_prime;
    bool &Delta = intermediate.Delta;
    ShareValue &Gamma = intermediate.Gamma;
    std::array<ShareValue, ell> &rx_list = intermediate.rx_list;
    constexpr ShareValue p = PI_sign_intermediate<ell, k>::p;

    ShareValue MASK_p = 1;
    int bitp = 1;
    while (MASK_p + 1 < p) {
        MASK_p = (MASK_p << 1) | 1;
        bitp++;
    }
    int bytep = (bitp + 7) / 8;

    ShareValue MASK_k = 1;
    int bitk = 1;
    while (MASK_k + 1 < k) {
        MASK_k = (MASK_k << 1) | 1;
        bitk++;
    }
    int bytek = (bitk + 7) / 8;

    // Step 0: P2将Gamma发送给P1
    if (party_id == 2) {
        netio.send_data(1, &Gamma, bytek);
    } else if (party_id == 1) {
        Gamma = 0;
        netio.recv_data(2, &Gamma, bytek);
    }

    if (party_id == 1 || party_id == 2) {
        // Step 1: 计算mx_list，将rx_list和mx_list最后一位设为0
        std::array<bool, ell> mx_list{};
        ShareValue mx_hat = (x.v1) & (x.MASK >> 1);
        // 第0个值是最高位，第ell-2个值是最低位
        for (int i = 0; i < ell - 1; i++) {
            mx_list[i] = (mx_hat >> (ell - 2 - i)) & 1;
        }
        mx_list[ell - 1] = 0;
        rx_list[ell - 1] = party_id - 1;

        // Step 2: 计算xor_list
        std::array<ShareValue, ell> xor_list{};
        for (int i = 0; i < ell; i++) {
            xor_list[i] = (rx_list[i] - 2 * mx_list[i] * rx_list[i] + 2 * p * p) % p;
            if (party_id == 1) {
                xor_list[i] = (xor_list[i] + mx_list[i]) % p;
            }
        }

        // Step3: 计算pre_list
        std::array<ShareValue, ell> pre_list{};
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
        std::array<ShareValue, ell> encode_list{};
        std::array<ShareValue, ell> w_list{};
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
        encoded_msg msg;
        for (int i = 0; i < ell; i++) {
            msg.add_msg((char *)&encode_list[i], bitp);
        }
        std::vector<char> tmp(msg.get_bytelen());
        msg.to_char_array(tmp.data());
        netio.send_data(0, tmp.data(), msg.get_bytelen());
    }

    if (party_id == 0) {
        // Step 6: P0接收并解码encode_list
        int bytelen = (ell * bitp + 7) / 8;

        encoded_msg msg1;
        std::vector<char> tmp1(bytelen);
        netio.recv_data(1, tmp1.data(), bytelen);
        msg1.from_char_array(tmp1.data(), bytelen);

        encoded_msg msg2;
        std::vector<char> tmp2(bytelen);
        netio.recv_data(2, tmp2.data(), bytelen);
        msg2.from_char_array(tmp2.data(), bytelen);

        std::array<ShareValue, ell> encode_list{};
        for (int i = 0; i < ell; i++) {
            ShareValue val = 0;
            msg1.read_msg((char *)&val, bitp);
            encode_list[i] = val;
        }
        for (int i = 0; i < ell; i++) {
            ShareValue val = 0;
            msg2.read_msg((char *)&val, bitp);
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