#include "protocol/detect.h"

// PI_detect 和 PI_prefix 的公共部分
void common_part(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 std::vector<ADDshare_p *> &input_bit_shares, std::vector<ADDshare_p *> &s) {
    std::vector<ADDshare_p *> &bit_shares = input_bit_shares;
    ShareValue p1 = input_bit_shares[0]->p;
    ShareValue p2 = s[0]->p;
    const int l = bit_shares.size();

    ShareValue MASK_p1 = 1;
    int bitp1 = 1;
    while (MASK_p1 + 1 < p1) {
        MASK_p1 = (MASK_p1 << 1) | 1;
        bitp1++;
    }
    int bytep1 = (bitp1 + 7) / 8;

    ShareValue MASK_p2 = 1;
    int bitp2 = 1;
    while (MASK_p2 + 1 < p2) {
        MASK_p2 = (MASK_p2 << 1) | 1;
        bitp2++;
    }
    int bytep2 = (bitp2 + 7) / 8;

    // Step 1: P1和P2计算：前缀和 - 2*原串 + 1
    std::vector<ShareValue> prefix_sums(l);
    if (party_id == 1 || party_id == 2) {
        ShareValue sum = 0;
        for (int i = 0; i < l; i++) {
            sum = (sum + bit_shares[i]->v) % p1;
            prefix_sums[i] = (sum + 2 * (-bit_shares[i]->v + p1)) % p1;
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
        encoded_msg msg;
        for (int i = 0; i < l; i++) {
            msg.add_msg((char *)&masked_values[i], bitp1);
        }
        std::vector<char> tmp(msg.get_bytelen());
        msg.to_char_array(tmp.data());
        netio.send_data(0, tmp.data(), msg.get_bytelen());
    } else {
        // 接收来自P1和P2的数据，并恢复成明文
        encoded_msg msg;
        int bytelen = (l * bitp1 + 7) / 8;
        std::vector<char> tmp(bytelen);
        for (int iparty = 1; iparty <= 2; iparty++) {
            msg.clear();
            netio.recv_data(iparty, tmp.data(), bytelen);
            msg.from_char_array(tmp.data(), bytelen);
            for (int i = 0; i < l; i++) {
                ShareValue val = 0;
                msg.read_msg((char *)&val, bitp1);
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
    if (party_id == 0 || party_id == 1) {
        for (int i = 0; i < l; i++) {
            PRGs[0].gen_random_data(&s[i]->v, sizeof(ShareValue));
            s[i]->v %= p2;
        }
    }
    if (party_id == 0) {
        ShareValue tmpv;
        encoded_msg msg;
        for (int i = 0; i < l; i++) {
            tmpv = (masked_values[i] + p2 - s[i]->v) % p2;
            msg.add_msg((char *)&tmpv, bitp2);
        }
        std::vector<char> tmp(msg.get_bytelen());
        msg.to_char_array(tmp.data());
        netio.send_data(2, tmp.data(), msg.get_bytelen());
    } else if (party_id == 2) {
        encoded_msg msg;
        int bytelen = (l * bitp2 + 7) / 8;
        std::vector<char> tmp(bytelen);
        netio.recv_data(0, tmp.data(), bytelen);
        msg.from_char_array(tmp.data(), bytelen);
        ShareValue tmpv = 0;
        for (int i = 0; i < l; i++) {
            msg.read_msg((char *)&tmpv, bitp2);
            s[i]->v = tmpv % p2;
        }
    }

    // Step 7: P1和P2应用逆排列pi_inv
    if (party_id == 1 || party_id == 2) {
        s = apply_permutation(pi_inv, s);
    }
}

void PI_detect(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               std::vector<ADDshare_p *> &input_bit_shares, ADDshare_p *output_result) {
#ifdef DEBUG_MODE
    ShareValue tmp = input_bit_shares[0]->p;
    for (size_t i = 1; i < input_bit_shares.size(); i++) {
        if (input_bit_shares[i]->p != tmp) {
            error("PI_detect: input_bit_shares have different p");
        }
        if (!input_bit_shares[i]->has_shared) {
            error("PI_detect: input_bit_shares not shared yet");
        }
    }
    output_result->has_shared = true;
#endif

    std::vector<ADDshare_p *> &bit_shares = input_bit_shares;
    ADDshare_p &result = *output_result;
    ShareValue p1 = input_bit_shares[0]->p;
    ShareValue p2 = output_result->p;
    const int l = bit_shares.size();

    std::vector<ADDshare_p> s(l, p2);
    std::vector<ADDshare_p *> s_ptrs = make_ptr_vec(s);
    common_part(party_id, PRGs, netio, bit_shares, s_ptrs);

    // Step 8: P1和P2将s的对应位置乘以index并求和
    result.v = 0;
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < l; i++) {
            result.v = (result.v + s_ptrs[i]->v * i) % p2;
        }
    }
}

void PI_prefix(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               std::vector<ADDshare_p *> &input_bit_shares,
               std::vector<ADDshare_p *> &output_result) {
#ifdef DEBUG_MODE
    if (input_bit_shares.size() != output_result.size()) {
        error("PI_prefix: input_bit_shares and output_result have different size");
    }
    ShareValue tmp = input_bit_shares[0]->p;
    for (size_t i = 0; i < input_bit_shares.size(); i++) {
        if (input_bit_shares[i]->p != tmp) {
            error("PI_prefix: input_bit_shares have different p");
        }
        if (!input_bit_shares[i]->has_shared) {
            error("PI_prefix: input_bit_shares not shared yet");
        }
    }

    tmp = output_result[0]->p;
    for (size_t i = 0; i < output_result.size(); i++) {
        if (output_result[i]->p != tmp) {
            error("PI_prefix: output_result have different p");
        }
        output_result[i]->has_shared = true;
    }
#endif

    std::vector<ADDshare_p *> &bit_shares = input_bit_shares;
    std::vector<ADDshare_p *> &result = output_result;
    ShareValue p1 = input_bit_shares[0]->p;
    ShareValue p2 = output_result[0]->p;
    const int l = bit_shares.size();

    std::vector<ADDshare_p> s(l, p2);
    std::vector<ADDshare_p *> s_ptrs = make_ptr_vec(s);
    common_part(party_id, PRGs, netio, bit_shares, s_ptrs);

    // Step 8: P1和P2对s从后向前做前缀和
    if (party_id == 1 || party_id == 2) {
        result[l - 1]->v = s_ptrs[l - 1]->v;
        for (int i = l - 2; i >= 0; i--) {
            result[i]->v = (result[i + 1]->v + s_ptrs[i]->v) % p2;
        }
    }
}

void PI_prefix_b(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 std::vector<ADDshare_p *> &input_bit_shares, MSSshare_p *input_b,
                 std::vector<ADDshare_p *> &output_result) {
#ifdef DEBUG_MODE
    if (input_bit_shares.size() != output_result.size()) {
        error("PI_prefix_b: input_bit_shares and output_result have different size");
    }
    ShareValue tmp = input_bit_shares[0]->p;
    for (size_t i = 0; i < input_bit_shares.size(); i++) {
        if (input_bit_shares[i]->p != tmp) {
            error("PI_prefix_b: input_bit_shares have different p");
        }
        if (!input_bit_shares[i]->has_shared) {
            error("PI_prefix_b: input_bit_shares not shared yet");
        }
    }

    tmp = output_result[0]->p;
    for (size_t i = 0; i < output_result.size(); i++) {
        if (output_result[i]->p != tmp) {
            error("PI_prefix_b: output_result have different p");
        }
        output_result[i]->has_shared = true;
    }
    if (input_b->p != tmp) {
        error("PI_prefix_b: b have different p");
    }
    if (!input_b->has_shared) {
        error("PI_prefix_b: b has not shared");
    }
#endif

    std::vector<ADDshare_p *> &bit_shares = input_bit_shares;
    std::vector<ADDshare_p *> &result = output_result;
    MSSshare_p &b = *input_b;
    ShareValue p1 = input_bit_shares[0]->p;
    ShareValue p2 = output_result[0]->p;
    const int l = bit_shares.size();

    ShareValue MASK_p1 = 1;
    int bitp1 = 1;
    while (MASK_p1 + 1 < p1) {
        MASK_p1 = (MASK_p1 << 1) | 1;
        bitp1++;
    }
    int bytep1 = (bitp1 + 7) / 8;

    ShareValue MASK_p2 = 1;
    int bitp2 = 1;
    while (MASK_p2 + 1 < p2) {
        MASK_p2 = (MASK_p2 << 1) | 1;
        bitp2++;
    }
    int bytep2 = (bitp2 + 7) / 8;

    // Step 1: P1和P2计算：前缀和 - 2*原串 + 1
    std::vector<ShareValue> prefix_sums(l);
    if (party_id == 1 || party_id == 2) {
        ShareValue sum = 0;
        for (int i = 0; i < l; i++) {
            sum = (sum + bit_shares[i]->v) % p1;
            prefix_sums[i] = (sum + 2 * (-bit_shares[i]->v + p1)) % p1;
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
        encoded_msg msg;
        for (int i = 0; i < l; i++) {
            msg.add_msg((char *)&masked_values[i], bitp1);
        }
        std::vector<char> tmp(msg.get_bytelen());
        msg.to_char_array(tmp.data());
        netio.send_data(0, tmp.data(), msg.get_bytelen());
    } else {
        // 接收来自P1和P2的数据，并恢复成明文
        encoded_msg msg;
        int bytelen = (l * bitp1 + 7) / 8;
        std::vector<char> tmp(bytelen);
        for (int iparty = 1; iparty <= 2; iparty++) {
            msg.clear();
            netio.recv_data(iparty, tmp.data(), bytelen);
            msg.from_char_array(tmp.data(), bytelen);
            for (int i = 0; i < l; i++) {
                ShareValue val = 0;
                msg.read_msg((char *)&val, bitp1);
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
    for (int idx = 0; idx < 2; idx++) {
        // Step 5: P0的masked_values应当恰好包含1个0，将该位置设为1/rb，其余位置设为0
        if (party_id == 0) {
            int cnt = 0;
            for (int i = 0; i < l; i++) {
                if (masked_values_copy[i] == 0) {
                    cnt++;
                    if (idx == 0)
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
                PRGs[0].gen_random_data(&s[idx][i].v, sizeof(ShareValue));
                s[idx][i].v %= p2;
            }
        }
        if (party_id == 0) {
            ShareValue tmpv;
            encoded_msg msg;
            for (int i = 0; i < l; i++) {
                tmpv = (masked_values[i] + p2 - s[idx][i].v) % p2;
                msg.add_msg((char *)&tmpv, bitp2);
            }
            std::vector<char> tmp(msg.get_bytelen());
            msg.to_char_array(tmp.data());
            netio.send_data(2, tmp.data(), msg.get_bytelen());
        } else if (party_id == 2) {
            encoded_msg msg;
            int bytelen = (l * bitp2 + 7) / 8;
            std::vector<char> tmp(bytelen);
            netio.recv_data(0, tmp.data(), bytelen);
            msg.from_char_array(tmp.data(), bytelen);
            ShareValue tmpv = 0;
            for (int i = 0; i < l; i++) {
                msg.read_msg((char *)&tmpv, bitp2);
                s[idx][i].v = tmpv % p2;
            }
        }

        // Step 7: P1和P2应用逆排列pi_inv
        if (party_id == 1 || party_id == 2) {
            s[idx] = apply_permutation(pi_inv, s[idx]);
        }
    }

    // Step 8: P1和P2对s[0]*mb+s[1]从后向前做前缀和
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < l; i++) {
            result[i]->v = (s[0][i].v * b.v1 + s[1][i].v) % p2;
        }
        for (int i = l - 2; i >= 0; i--) {
            result[i]->v = (result[i + 1]->v + result[i]->v) % p2;
        }
    }
}
