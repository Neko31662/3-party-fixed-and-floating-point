void PI_detect(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               std::vector<ADDshare_p> &input_bit_shares, ADDshare_p *output_result) {
#ifdef DEBUG_MODE
    ShareValue tmp = input_bit_shares[0].p;
    for (size_t i = 1; i < input_bit_shares.size(); i++) {
        if (input_bit_shares[i].p != tmp) {
            error("PI_detect: input_bit_shares have different p");
        }
        if (!input_bit_shares[i].has_shared && party_id != 0) {
            error("PI_detect: input_bit_shares not shared yet");
        }
    }
    output_result->has_shared = true;
#endif

    std::vector<ADDshare_p> &bit_shares = input_bit_shares;
    ADDshare_p &result = *output_result;
    ShareValue &p1 = input_bit_shares[0].p;
    ShareValue &p2 = output_result->p;
    const int l = bit_shares.size();
    int bytep1 = byte_of(p1);
    int bytep2 = byte_of(p2);

    // Step 1: P1和P2计算：前缀和 - 2*原串 + 1
    std::vector<ShareValue> prefix_sums(l);
    if (party_id == 1 || party_id == 2) {
        ShareValue sum = 0;
        for (int i = 0; i < l; i++) {
            sum = (sum + bit_shares[i].v) % p1;
            prefix_sums[i] = (sum + 2 * (-bit_shares[i].v + p1)) % p1;
            if (party_id == 1)
                prefix_sums[i] = (prefix_sums[i] + 1) % p1;
        }
    }

    // Step 2: P1和P2随机同步生成l个1~p1的值w
    std::vector<ShareValue> w(l);
    if (party_id == 1 || party_id == 2) {
        PRGs[1].gen_random_data(w.data(), l * sizeof(ShareValue));
        for (int i = 0; i < l; i++) {
            w[i] = (w[i] % (p1 - 1)) + 1; // 确保w在1~p1-1之间
        }
    }

    // Step 3: P1和P2随机同步生成一个长度为l排列pi，以及它的逆排列pi_inv
    std::vector<int> pi, pi_inv;
    if (party_id == 1 || party_id == 2) {
        pi = gen_random_permutation(l, PRGs[1]);
        pi_inv = inv_permutation(pi);
    }

    // Step 4: P1和P2按位置计算prefix_sums * w并打乱，将数据发送给P0
    std::vector<ShareValue> masked_values(l, 0);
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < l; i++) {
            masked_values[i] = (prefix_sums[i] * w[i]) % p1;
        }
        masked_values = apply_permutation(pi, masked_values);
        for (int i = 0; i < l; i++) {
            netio.store_data(0, (char *)&masked_values[i], bytep1);
        }
        netio.send_stored_data(0);
    } else {
        // 接收来自P1和P2的数据，并恢复成明文
        for (int iparty = 1; iparty <= 2; iparty++) {
            for (int i = 0; i < l; i++) {
                ShareValue val = 0;
                netio.recv_data(iparty, (char *)&val, bytep1);
                masked_values[i] += val;
            }
        }
        for (int i = 0; i < l; i++) {
            masked_values[i] %= p1;
        }
    }

    // Step 5: P0的masked_values应当恰好包含1个0，将该位置设为1，其余位置设为0
    if (party_id == 0) {
        int cnt = 0;
        for (int i = 0; i < l; i++) {
            if (masked_values[i] == 0) {
                cnt++;
                masked_values[i] = 1;
            } else {
                masked_values[i] = 0;
            }
        }
        if (cnt > 1) {
            error("PI_detect/PI_prefix: P0 find more than one zeros in masked_values");
        }
    }

    // Step 6: P0将上一步的结果（记为s）分享给P1和P2
    std::vector<ADDshare_p> s(l, p2);
    if (party_id == 0 || party_id == 1) {
        for (int i = 0; i < l; i++) {
            PRGs[0].gen_random_data(&s[i].v, sizeof(ShareValue));
            s[i].v %= p2;
        }
    }
    if (party_id == 0) {
        ShareValue tmpv;
        for (int i = 0; i < l; i++) {
            tmpv = (masked_values[i] + p2 - s[i].v) % p2;
            netio.store_data(2, (char *)&tmpv, bytep2);
        }
        netio.send_stored_data(2);
    } else if (party_id == 2) {
        ShareValue tmpv = 0;
        for (int i = 0; i < l; i++) {
            netio.recv_data(0, (char *)&tmpv, bytep2);
            s[i].v = tmpv % p2;
        }
    }

    // Step 7: P1和P2应用逆排列pi_inv
    if (party_id == 1 || party_id == 2) {
        s = apply_permutation(pi_inv, s);
    }

    // Step 8: P1和P2将s的对应位置乘以index并求和
    result.v = 0;
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < l; i++) {
            result.v = (result.v + s[i].v * i) % p2;
        }
    }
}

void PI_detect_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   std::vector<std::vector<ADDshare_p> *> &input_bit_shares_vec,
                   std::vector<ADDshare_p *> &output_result_vec) {
#ifdef DEBUG_MODE
    if (input_bit_shares_vec.size() != output_result_vec.size()) {
        error("PI_detect: input_bit_shares_vec and output_result_vec size mismatch");
    }
    for (size_t idx = 0; idx < input_bit_shares_vec.size(); idx++) {
        auto &input_bit_shares = *(input_bit_shares_vec[idx]);
        auto &output_result = output_result_vec[idx];
        ShareValue tmp = input_bit_shares[0].p;
        for (size_t i = 1; i < input_bit_shares.size(); i++) {
            if (input_bit_shares[i].p != tmp) {
                error("PI_detect: input_bit_shares have different p");
            }
            if (!input_bit_shares[i].has_shared && party_id != 0) {
                error("PI_detect: input_bit_shares not shared yet");
            }
        }
        output_result->has_shared = true;
    }
#endif

    int vec_size = input_bit_shares_vec.size();
    int bytep1_vec[vec_size], bytep2_vec[vec_size];
    for (int idx = 0; idx < vec_size; idx++) {
        auto &input_bit_shares = *(input_bit_shares_vec[idx]);
        auto &output_result = output_result_vec[idx];
        ShareValue &p1 = input_bit_shares[0].p;
        ShareValue &p2 = output_result->p;
        bytep1_vec[idx] = byte_of(p1);
        bytep2_vec[idx] = byte_of(p2);
    }

    std::vector<std::vector<int>> pi_inv_vec;
    if (party_id == 1 || party_id == 2) {
        pi_inv_vec.reserve(vec_size);
        for (int idx = 0; idx < vec_size; idx++) {
            auto &input_bit_shares = *(input_bit_shares_vec[idx]);
            auto &output_result = output_result_vec[idx];
            std::vector<ADDshare_p> &bit_shares = input_bit_shares;
            ADDshare_p &result = *output_result;
            ShareValue &p1 = input_bit_shares[0].p;
            ShareValue &p2 = output_result->p;
            const int l = bit_shares.size();
            int &bytep1 = bytep1_vec[idx];
            int &bytep2 = bytep2_vec[idx];

            // Step 1: P1和P2计算：前缀和 - 2*原串 + 1
            std::vector<ShareValue> prefix_sums(l);
            ShareValue sum = 0;
            for (int i = 0; i < l; i++) {
                sum = (sum + bit_shares[i].v) % p1;
                prefix_sums[i] = (sum + 2 * (-bit_shares[i].v + p1)) % p1;
                if (party_id == 1)
                    prefix_sums[i] = (prefix_sums[i] + 1) % p1;
            }

            // Step 2: P1和P2随机同步生成l个1~p1的值w
            std::vector<ShareValue> w(l);
            PRGs[1].gen_random_data(w.data(), l * sizeof(ShareValue));
            for (int i = 0; i < l; i++) {
                w[i] = (w[i] % (p1 - 1)) + 1; // 确保w在1~p1-1之间
            }

            // Step 3: P1和P2随机同步生成一个长度为l排列pi，以及它的逆排列pi_inv
            std::vector<int> pi, pi_inv;
            pi = gen_random_permutation(l, PRGs[1]);
            pi_inv = inv_permutation(pi);

            // Step 4: P1和P2按位置计算prefix_sums * w并打乱，将数据发送给P0
            std::vector<ShareValue> masked_values(l, 0);
            for (int i = 0; i < l; i++) {
                masked_values[i] = (prefix_sums[i] * w[i]) % p1;
            }
            masked_values = apply_permutation(pi, masked_values);
            for (int i = 0; i < l; i++) {
                netio.store_data(0, (char *)&masked_values[i], bytep1);
            }

            pi_inv_vec.push_back(pi_inv);
        }
        netio.send_stored_data(0);
    }

    if (party_id == 0) {
        for (int idx = 0; idx < vec_size; idx++) {
            auto &input_bit_shares = *(input_bit_shares_vec[idx]);
            auto &output_result = output_result_vec[idx];
            std::vector<ADDshare_p> &bit_shares = input_bit_shares;
            ADDshare_p &result = *output_result;
            ShareValue &p1 = input_bit_shares[0].p;
            ShareValue &p2 = output_result->p;
            const int l = bit_shares.size();
            int &bytep1 = bytep1_vec[idx];
            int &bytep2 = bytep2_vec[idx];

            // 接收来自P1和P2的数据，并恢复成明文
            std::vector<ShareValue> masked_values(l, 0);
            for (int iparty = 1; iparty <= 2; iparty++) {
                for (int i = 0; i < l; i++) {
                    ShareValue val = 0;
                    netio.recv_data(iparty, (char *)&val, bytep1);
                    masked_values[i] += val;
                }
            }
            for (int i = 0; i < l; i++) {
                masked_values[i] %= p1;
            }

            // Step 5: P0的masked_values应当恰好包含1个0，将该位置设为1，其余位置设为0
            if (party_id == 0) {
                int cnt = 0;
                for (int i = 0; i < l; i++) {
                    if (masked_values[i] == 0) {
                        cnt++;
                        masked_values[i] = 1;
                    } else {
                        masked_values[i] = 0;
                    }
                }
                if (cnt > 1) {
                    error("PI_detect/PI_prefix: P0 find more than one zeros in masked_values");
                }
            }

            std::vector<ADDshare_p> s(l, p2);
            for (int i = 0; i < l; i++) {
                PRGs[0].gen_random_data(&s[i].v, sizeof(ShareValue));
                s[i].v %= p2;
            }
            ShareValue tmpv;
            for (int i = 0; i < l; i++) {
                tmpv = (masked_values[i] + p2 - s[i].v) % p2;
                netio.store_data(2, (char *)&tmpv, bytep2);
            }
        }
        netio.send_stored_data(2);
    }

    if (party_id == 1 || party_id == 2) {
        for (int idx = 0; idx < vec_size; idx++) {
            auto &input_bit_shares = *(input_bit_shares_vec[idx]);
            auto &output_result = output_result_vec[idx];
            std::vector<ADDshare_p> &bit_shares = input_bit_shares;
            ADDshare_p &result = *output_result;
            ShareValue &p1 = input_bit_shares[0].p;
            ShareValue &p2 = output_result->p;
            const int l = bit_shares.size();
            int &bytep1 = bytep1_vec[idx];
            int &bytep2 = bytep2_vec[idx];

            std::vector<ADDshare_p> s(l, p2);
            if (party_id == 1) {
                for (int i = 0; i < l; i++) {
                    PRGs[0].gen_random_data(&s[i].v, sizeof(ShareValue));
                    s[i].v %= p2;
                }
            } else if (party_id == 2) {
                ShareValue tmpv = 0;
                for (int i = 0; i < l; i++) {
                    netio.recv_data(0, (char *)&tmpv, bytep2);
                    s[i].v = tmpv % p2;
                }
            }

            // Step 7: P1和P2应用逆排列pi_inv
            if (party_id == 1 || party_id == 2) {
                s = apply_permutation(pi_inv_vec[idx], s);
            }

            // Step 8: P1和P2将s的对应位置乘以index并求和
            result.v = 0;
            if (party_id == 1 || party_id == 2) {
                for (int i = 0; i < l; i++) {
                    result.v = (result.v + s[i].v * i) % p2;
                }
            }
        }
    }
}

void PI_prefix_b(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 std::vector<ADDshare_p> &input_bit_shares, MSSshare_p *input_b,
                 std::vector<ADDshare_p> &output_result) {
#ifdef DEBUG_MODE
    if (input_bit_shares.size() != output_result.size()) {
        error("PI_prefix_b: input_bit_shares and output_result have different size");
    }
    ShareValue tmp = input_bit_shares[0].p;
    for (size_t i = 0; i < input_bit_shares.size(); i++) {
        if (input_bit_shares[i].p != tmp) {
            error("PI_prefix_b: input_bit_shares have different p");
        }
        if (!input_bit_shares[i].has_shared && party_id != 0) {
            error("PI_prefix_b: input_bit_shares not shared yet");
        }
    }

    tmp = output_result[0].p;
    for (size_t i = 0; i < output_result.size(); i++) {
        if (output_result[i].p != tmp) {
            error("PI_prefix_b: output_result have different p");
        }
        output_result[i].has_shared = true;
    }
    if (input_b->p != tmp) {
        error("PI_prefix_b: b have different p");
    }
    if (!input_b->has_shared) {
        error("PI_prefix_b: b has not shared");
    }
#endif

    std::vector<ADDshare_p> &bit_shares = input_bit_shares;
    std::vector<ADDshare_p> &result = output_result;
    MSSshare_p &b = *input_b;
    ShareValue &p1 = input_bit_shares[0].p;
    ShareValue &p2 = output_result[0].p;
    const int l = bit_shares.size();
    int bytep1 = byte_of(p1);
    int bytep2 = byte_of(p2);

    // Step 1: P1和P2计算：前缀和 - 2*原串 + 1
    std::vector<ShareValue> prefix_sums(l);
    if (party_id == 1 || party_id == 2) {
        ShareValue sum = 0;
        for (int i = 0; i < l; i++) {
            sum = (sum + bit_shares[i].v) % p1;
            prefix_sums[i] = (sum + 2 * (-bit_shares[i].v + p1)) % p1;
            if (party_id == 1)
                prefix_sums[i] = (prefix_sums[i] + 1) % p1;
        }
    }

    // Step 2: P1和P2随机同步生成l个1~p1的值w
    std::vector<ShareValue> w(l);
    if (party_id == 1 || party_id == 2) {
        PRGs[1].gen_random_data(w.data(), l * sizeof(ShareValue));
        for (int i = 0; i < l; i++) {
            w[i] = (w[i] % (p1 - 1)) + 1; // 确保w在1~p1-1之间
        }
    }

    // Step 3: P1和P2随机同步生成一个长度为l排列pi，以及它的逆排列pi_inv
    std::vector<int> pi, pi_inv;
    if (party_id == 1 || party_id == 2) {
        pi = gen_random_permutation(l, PRGs[1]);
        pi_inv = inv_permutation(pi);
    }

    // Step 4: P1和P2按位置计算prefix_sums * w并打乱，将数据发送给P0
    std::vector<ShareValue> masked_values(l, 0);
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < l; i++) {
            masked_values[i] = (prefix_sums[i] * w[i]) % p1;
        }
        masked_values = apply_permutation(pi, masked_values);
        for (int i = 0; i < l; i++) {
            netio.store_data(0, (char *)&masked_values[i], bytep1);
        }
        netio.send_stored_data(0);
    } else {
        // 接收来自P1和P2的数据，并恢复成明文
        for (int iparty = 1; iparty <= 2; iparty++) {
            ShareValue val = 0;
            for (int i = 0; i < l; i++) {
                netio.recv_data(iparty, (char *)&val, bytep1);
                masked_values[i] += val;
            }
        }
        for (int i = 0; i < l; i++) {
            masked_values[i] %= p1;
        }
    }

    std::vector<ADDshare_p> s[2] = {std::vector<ADDshare_p>(l, p2), std::vector<ADDshare_p>(l, p2)};
    auto masked_values_copy = masked_values;

    // 重复两次，第一次分享值为1，第二次分享值为rb
    for (int s_idx = 0; s_idx < 2; s_idx++) {
        // Step 5: P0的masked_values应当恰好包含1个0，将该位置设为1/rb，其余位置设为0
        if (party_id == 0) {
            int cnt = 0;
            for (int i = 0; i < l; i++) {
                if (masked_values_copy[i] == 0) {
                    cnt++;
                    if (s_idx == 0)
                        masked_values[i] = 1;
                    else {
                        ShareValue r = (b.v1 + b.v2) % b.p;
                        masked_values[i] = r;
                    }
                } else {
                    masked_values[i] = 0;
                }
            }
            if (cnt > 1) {
                error("PI_prefix_b: P0 find more than one zeros in masked_values");
            }
        }

        // Step 6: P0将上一步的结果（记为s）分享给P1和P2
        if (party_id == 0 || party_id == 1) {
            for (int i = 0; i < l; i++) {
                PRGs[0].gen_random_data(&s[s_idx][i].v, sizeof(ShareValue));
                s[s_idx][i].v %= p2;
            }
        }
        if (party_id == 0) {
            ShareValue tmpv;
            for (int i = 0; i < l; i++) {
                tmpv = (masked_values[i] + p2 - s[s_idx][i].v) % p2;
                netio.store_data(2, (char *)&tmpv, bytep2);
            }
            netio.send_stored_data(2);
        } else if (party_id == 2) {
            ShareValue tmpv = 0;
            for (int i = 0; i < l; i++) {
                netio.recv_data(0, (char *)&tmpv, bytep2);
                s[s_idx][i].v = tmpv % p2;
            }
        }

        // Step 7: P1和P2应用逆排列pi_inv
        if (party_id == 1 || party_id == 2) {
            s[s_idx] = apply_permutation(pi_inv, s[s_idx]);
        }
    }

    // Step 8: P1和P2对s[0]*mb+s[1]从后向前做前缀和
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < l; i++) {
            result[i].v = (s[0][i].v * b.v1 + s[1][i].v) % p2;
        }
        for (int i = l - 2; i >= 0; i--) {
            result[i].v = (result[i + 1].v + result[i].v) % p2;
        }
    }
}

void PI_prefix_b_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     std::vector<std::vector<ADDshare_p> *> &input_bit_shares_vec,
                     std::vector<MSSshare_p *> &input_b_vec,
                     std::vector<std::vector<ADDshare_p> *> &output_result_vec) {
#ifdef DEBUG_MODE
    if (input_bit_shares_vec.size() != output_result_vec.size() ||
        input_bit_shares_vec.size() != input_b_vec.size() ||
        input_bit_shares_vec.size() != output_result_vec.size()) {
        error("PI_prefix_b_vec: input size mismatch");
    }
    for (size_t idx = 0; idx < input_bit_shares_vec.size(); idx++) {
        auto &input_bit_shares = *(input_bit_shares_vec[idx]);
        auto &output_result = *(output_result_vec[idx]);
        auto &input_b = input_b_vec[idx];

        if (input_bit_shares.size() != output_result.size()) {
            error("PI_prefix_b_vec: input_bit_shares and output_result have different size");
        }
        ShareValue tmp = input_bit_shares[0].p;
        for (size_t i = 0; i < input_bit_shares.size(); i++) {
            if (input_bit_shares[i].p != tmp) {
                error("PI_prefix_b_vec: input_bit_shares have different p");
            }
            if (!input_bit_shares[i].has_shared && party_id != 0) {
                error("PI_prefix_b_vec: input_bit_shares not shared yet");
            }
        }

        tmp = output_result[0].p;
        for (size_t i = 0; i < output_result.size(); i++) {
            if (output_result[i].p != tmp) {
                error("PI_prefix_b_vec: output_result have different p");
            }
            output_result[i].has_shared = true;
        }
        if (input_b->p != tmp) {
            error("PI_prefix_b_vec: b have different p");
        }
        if (!input_b->has_shared) {
            error("PI_prefix_b_vec: b has not shared");
        }
    }
#endif

    int vec_size = input_bit_shares_vec.size();
    int bytep1_vec[vec_size], bytep2_vec[vec_size];
    for (int idx = 0; idx < vec_size; idx++) {
        auto &input_bit_shares = *(input_bit_shares_vec[idx]);
        auto &output_result = *(output_result_vec[idx]);
        auto &input_b = input_b_vec[idx];
        std::vector<ADDshare_p> &bit_shares = input_bit_shares;
        std::vector<ADDshare_p> &result = output_result;
        MSSshare_p &b = *input_b;
        ShareValue &p1 = input_bit_shares[0].p;
        ShareValue &p2 = output_result[0].p;
        bytep1_vec[idx] = byte_of(p1);
        bytep2_vec[idx] = byte_of(p2);
    }

    std::vector<std::vector<int>> pi_inv_vec;
    if (party_id == 1 || party_id == 2) {
        pi_inv_vec.reserve(vec_size);
        for (int idx = 0; idx < vec_size; idx++) {
            auto &input_bit_shares = *(input_bit_shares_vec[idx]);
            auto &output_result = *(output_result_vec[idx]);
            auto &input_b = input_b_vec[idx];
            std::vector<ADDshare_p> &bit_shares = input_bit_shares;
            std::vector<ADDshare_p> &result = output_result;
            MSSshare_p &b = *input_b;
            ShareValue &p1 = input_bit_shares[0].p;
            ShareValue &p2 = output_result[0].p;
            const int l = bit_shares.size();
            int &bytep1 = bytep1_vec[idx];
            int &bytep2 = bytep2_vec[idx];

            // Step 1: P1和P2计算：前缀和 - 2*原串 + 1
            std::vector<ShareValue> prefix_sums(l);
            ShareValue sum = 0;
            for (int i = 0; i < l; i++) {
                sum = (sum + bit_shares[i].v) % p1;
                prefix_sums[i] = (sum + 2 * (-bit_shares[i].v + p1)) % p1;
                if (party_id == 1)
                    prefix_sums[i] = (prefix_sums[i] + 1) % p1;
            }

            // Step 2: P1和P2随机同步生成l个1~p1的值w
            std::vector<ShareValue> w(l);
            PRGs[1].gen_random_data(w.data(), l * sizeof(ShareValue));
            for (int i = 0; i < l; i++) {
                w[i] = (w[i] % (p1 - 1)) + 1; // 确保w在1~p1-1之间
            }

            // Step 3: P1和P2随机同步生成一个长度为l排列pi，以及它的逆排列pi_inv
            std::vector<int> pi, pi_inv;
            pi = gen_random_permutation(l, PRGs[1]);
            pi_inv = inv_permutation(pi);

            // Step 4: P1和P2按位置计算prefix_sums * w并打乱，将数据发送给P0
            std::vector<ShareValue> masked_values(l, 0);
            for (int i = 0; i < l; i++) {
                masked_values[i] = (prefix_sums[i] * w[i]) % p1;
            }
            masked_values = apply_permutation(pi, masked_values);
            for (int i = 0; i < l; i++) {
                netio.store_data(0, (char *)&masked_values[i], bytep1);
            }

            pi_inv_vec.push_back(pi_inv);
        }
        netio.send_stored_data(0);
    }

    for (int idx = 0; idx < vec_size; idx++) {
        auto &input_bit_shares = *(input_bit_shares_vec[idx]);
        auto &output_result = *(output_result_vec[idx]);
        auto &input_b = input_b_vec[idx];
        std::vector<ADDshare_p> &bit_shares = input_bit_shares;
        std::vector<ADDshare_p> &result = output_result;
        MSSshare_p &b = *input_b;
        ShareValue &p1 = input_bit_shares[0].p;
        ShareValue &p2 = output_result[0].p;
        const int l = bit_shares.size();
        int &bytep1 = bytep1_vec[idx];
        int &bytep2 = bytep2_vec[idx];

        // P0接收来自P1和P2的数据，并恢复成明文
        std::vector<ShareValue> masked_values(l, 0);
        if (party_id == 0) {
            for (int iparty = 1; iparty <= 2; iparty++) {
                ShareValue val = 0;
                for (int i = 0; i < l; i++) {
                    netio.recv_data(iparty, (char *)&val, bytep1);
                    masked_values[i] += val;
                }
            }
            for (int i = 0; i < l; i++) {
                masked_values[i] %= p1;
            }
        }

        std::vector<ADDshare_p> s[2] = {std::vector<ADDshare_p>(l, p2),
                                        std::vector<ADDshare_p>(l, p2)};
        auto masked_values_copy = masked_values;

        // 重复两次，第一次分享值为1，第二次分享值为rb
        for (int s_idx = 0; s_idx < 2; s_idx++) {
            // Step 5: P0的masked_values应当恰好包含1个0，将该位置设为1/rb，其余位置设为0
            if (party_id == 0) {
                int cnt = 0;
                for (int i = 0; i < l; i++) {
                    if (masked_values_copy[i] == 0) {
                        cnt++;
                        if (s_idx == 0)
                            masked_values[i] = 1;
                        else {
                            ShareValue r = (b.v1 + b.v2) % b.p;
                            masked_values[i] = r;
                        }
                    } else {
                        masked_values[i] = 0;
                    }
                }
                if (cnt > 1) {
                    error("PI_prefix_b: P0 find more than one zeros in masked_values");
                }
            }

            // Step 6: P0将上一步的结果（记为s）分享给P1和P2
            if (party_id == 0 || party_id == 1) {
                for (int i = 0; i < l; i++) {
                    PRGs[0].gen_random_data(&s[s_idx][i].v, sizeof(ShareValue));
                    s[s_idx][i].v %= p2;
                }
            }
            if (party_id == 0) {
                ShareValue tmpv;
                for (int i = 0; i < l; i++) {
                    tmpv = (masked_values[i] + p2 - s[s_idx][i].v) % p2;
                    netio.store_data(2, (char *)&tmpv, bytep2);
                }
            } else if (party_id == 2) {
                ShareValue tmpv = 0;
                for (int i = 0; i < l; i++) {
                    netio.recv_data(0, (char *)&tmpv, bytep2);
                    s[s_idx][i].v = tmpv % p2;
                }
            }

            // Step 7: P1和P2应用逆排列pi_inv
            if (party_id == 1 || party_id == 2) {
                s[s_idx] = apply_permutation(pi_inv_vec[idx], s[s_idx]);
            }
        }

        // Step 8: P1和P2对s[0]*mb+s[1]从后向前做前缀和
        if (party_id == 1 || party_id == 2) {
            for (int i = 0; i < l; i++) {
                result[i].v = (s[0][i].v * b.v1 + s[1][i].v) % p2;
            }
            for (int i = l - 2; i >= 0; i--) {
                result[i].v = (result[i + 1].v + result[i].v) % p2;
            }
        }
    }
    if (party_id == 0) {
        netio.send_stored_data(2);
    }
}
