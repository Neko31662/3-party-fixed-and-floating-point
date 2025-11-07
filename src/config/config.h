#pragma once

#ifndef GLOBAL_CONFIG
#define GLOBAL_CONFIG
#include <cstdint>

#define LOCALHOST
#define DEBUG_MODE
#define __MORE_FLUSH
inline const char *IP[] = {"127.0.0.1", "127.0.0.1", "127.0.0.1"};

typedef uint64_t ShareValue;
inline const int ShareValue_BitLength = sizeof(ShareValue) * 8;
typedef __uint128_t LongShareValue;
inline const int LongShareValue_BitLength = sizeof(LongShareValue) * 8;

#endif