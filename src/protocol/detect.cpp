#include "protocol/detect.h"

// PI_detect 和 PI_prefix 的公共部分
void common_part(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 const std::vector<ShareValue> &input_bit_shares, const ShareValue input_p1,
                 const ShareValue input_p2, std::vector<ShareValue> &s) {
    auto &bit_shares = input_bit_shares;
    auto &p1 = input_p1;
    auto &p2 = input_p2;

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

    const int l = bit_shares.size();

    // Step 1: P1和P2计算：前缀和 - 2*原串 + 1
    std::vector<ShareValue> prefix_sums(l);
    if (party_id == 1 || party_id == 2) {
        ShareValue sum = 0;
        for (int i = 0; i < l; i++) {
            sum = (sum + bit_shares[i]) % p1;
            prefix_sums[i] = (sum + 2 * (-bit_shares[i] + p1)) % p1;
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
    char seed[8];
    if (party_id == 1 || party_id == 2) {
        PRGs[1].gen_random_data(seed, 8);
        pi = gen_random_permutation(l, seed);
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
        PRGs[0].gen_random_data(s.data(), l * sizeof(ShareValue));
        for (int i = 0; i < l; i++) {
            s[i] %= p2;
        }
    }
    if (party_id == 0) {
        ShareValue tmpv;
        encoded_msg msg;
        for (int i = 0; i < l; i++) {
            tmpv = (masked_values[i] + p2 - s[i]) % p2;
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
        ShareValue tmpv;
        for (int i = 0; i < l; i++) {
            msg.read_msg((char *)&tmpv, bitp2);
            s[i] = tmpv % p2;
        }
    }

    // Step 7: P1和P2应用逆排列pi_inv
    if (party_id == 1 || party_id == 2) {
        s = apply_permutation(pi_inv, s);
    }
}

void PI_detect(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               const std::vector<ShareValue> &input_bit_shares, const ShareValue input_p1,
               const ShareValue input_p2, ShareValue &output_result) {
    auto &bit_shares = input_bit_shares;
    auto &p1 = input_p1;
    auto &p2 = input_p2;
    auto &result = output_result;
    const int l = bit_shares.size();

    std::vector<ShareValue> s(l);
    common_part(party_id, PRGs, netio, bit_shares, p1, p2, s);

    // Step 8: P1和P2将s的对应位置乘以index并求和
    result = 0;
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < l; i++) {
            result = (result + s[i] * i) % p2;
        }
    }
}

void PI_prefix(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               const std::vector<ShareValue> &input_bit_shares, const ShareValue input_p1,
               const ShareValue input_p2, std::vector<ShareValue> &output_result) {
    auto &bit_shares = input_bit_shares;
    auto &p1 = input_p1;
    auto &p2 = input_p2;
    auto &result = output_result;
    const int l = bit_shares.size();

    std::vector<ShareValue> s(l);
    common_part(party_id, PRGs, netio, bit_shares, p1, p2, s);

    // Step 8: P1和P2对s从后向前做前缀和
    if (party_id == 1 || party_id == 2) {
        result[l-1] = s[l-1];
        for (int i = l - 2; i >= 0; i--) {
            result[i] = (result[i + 1] + s[i]) % p2;
        }
    }
}
