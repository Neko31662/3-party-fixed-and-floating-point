#pragma once
#include "config/config.h"
#include "utils/multi_party_net_io.h"
#include <cstdio>
#include <iostream>
#include <string>

inline void error(std::string msg) {
    std::cout << "ERROR: " << msg << std::endl;
    exit(1);
}

inline void log(std::string msg) {
    fflush(stderr);
    fflush(stdout);
    std::cout << msg << std::endl;
    fflush(stdout);
}

// 计算最高位 1 的位置（最低位为1）
constexpr static inline int highest_bit_pos(uint64_t x) {
    assert(x != 0);
    int pos = 0;
    while (x) {
        ++pos;
        x >>= 1;
    }
    return pos;
}

struct encoded_msg {
    std::vector<bool> data;
    int read_pos = 0;

    void add_msg(const char *msg, size_t bitlen) {
        for (size_t i = 0; i < bitlen; i++) {
            size_t byte_pos = i / 8;
            size_t bit_pos = i % 8;
            bool bit = (msg[byte_pos] >> bit_pos) & 1;
            data.push_back(bit);
        }
    }

    void read_msg(char *msg, size_t bitlen) {
        for (size_t i = 0; i < bitlen; i++) {
            size_t byte_pos = i / 8;
            size_t bit_pos = i % 8;
#ifdef DEBUG_MODE
            if (read_pos >= data.size()) {
                error("encoded_msg::read_msg: read beyond data size");
            }
#endif
            bool bit = data[read_pos++];
            if (bit) {
                msg[byte_pos] |= (1 << bit_pos);
            } else {
                msg[byte_pos] &= ~(1 << bit_pos);
            }
        }
    }

    int to_char_array(char *out) {
        size_t bytelen = (data.size() + 7) / 8;
        for (size_t i = 0; i < bytelen; i++) {
            out[i] = 0;
        }
        for (size_t i = 0; i < data.size(); i++) {
            size_t byte_pos = i / 8;
            size_t bit_pos = i % 8;
            if (data[i]) {
                out[byte_pos] |= (1 << bit_pos);
            }
        }
        return bytelen;
    }

    void from_char_array(const char *in, size_t bytelen) {
        data.clear();
        int bitlen = bytelen * 8;
        for (size_t i = 0; i < bitlen; i++) {
            size_t byte_pos = i / 8;
            size_t bit_pos = i % 8;
            bool bit = (in[byte_pos] >> bit_pos) & 1;
            data.push_back(bit);
        }
    }

    void clear() {
        data.clear();
        read_pos = 0;
    }

    inline size_t get_bytelen() { return (data.size() + 7) / 8; }

    inline size_t get_bitlen() { return data.size(); }
};