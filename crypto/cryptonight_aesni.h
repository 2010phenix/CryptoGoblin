/*
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  *
  */
#pragma once

#include "cryptonight.h"
#include "keccak.hpp"
#include "../common.h"
#include <memory.h>
#include <stdio.h>

#if defined(__GNUC__)
ALWAYS_INLINE static inline uint64_t _xmr_umul128(uint64_t a, uint64_t b, uint64_t* hi)
{
    unsigned __int128 r = (unsigned __int128)a * (unsigned __int128)b;
    *hi = r >> 64;
    return (uint64_t)r;
}
#endif

#if defined(__GNUC__)
# if defined(_WIN64)
#  include <intrin.h>
# else
#  include <x86intrin.h>
# endif
# define _umul128 _xmr_umul128
# define _mm256_set_m128i(v0, v1)  _mm256_insertf128_si256(_mm256_castsi128_si256(v1), (v0), 1)
#else
# include <intrin.h>
#endif // __GNUC__


#if !defined(_LP64) && !defined(_WIN64)
#error You are trying to do a 32-bit build. This will all end in tears. I know it.
#endif

extern void(*const extra_hashes[4])(const void *, char *);

__m128i soft_aesenc(__m128i in, __m128i key);
__m128i soft_aeskeygenassist(__m128i key, uint8_t rcon);

// This will shift and xor tmp1 into itself as 4 32-bit vals such as
// sl_xor(a1 a2 a3 a4) = a1 (a2^a1) (a3^a2^a1) (a4^a3^a2^a1)
ALWAYS_INLINE FLATTEN static inline __m128i sl_xor(__m128i tmp1)
{
    __m128i tmp4;
    tmp4 = _mm_slli_si128(tmp1, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    return tmp1;
}

#pragma GCC target ("sse4.2")
template<uint8_t rcon>
ALWAYS_INLINE FLATTEN static inline void aes_genkey_sub(__m128i* xout0, __m128i* xout2)
{
    __m128i xout1 = _mm_aeskeygenassist_si128(*xout2, rcon);
    xout1 = _mm_shuffle_epi32(xout1, 0xFF); // see PSHUFD, set all elems to 4th elem
    *xout0 = sl_xor(*xout0);
    *xout0 = _mm_xor_si128(*xout0, xout1);
    xout1 = _mm_aeskeygenassist_si128(*xout0, 0x00);
    xout1 = _mm_shuffle_epi32(xout1, 0xAA); // see PSHUFD, set all elems to 3rd elem
    *xout2 = sl_xor(*xout2);
    *xout2 = _mm_xor_si128(*xout2, xout1);
}
#pragma GCC reset_options

template<uint8_t rcon>
ALWAYS_INLINE FLATTEN static inline void soft_aes_genkey_sub(__m128i* xout0, __m128i* xout2)
{
    __m128i xout1 = soft_aeskeygenassist(*xout2, rcon);
    xout1 = _mm_shuffle_epi32(xout1, 0xFF); // see PSHUFD, set all elems to 4th elem
    *xout0 = sl_xor(*xout0);
    *xout0 = _mm_xor_si128(*xout0, xout1);
    xout1 = soft_aeskeygenassist(*xout0, 0x00);
    xout1 = _mm_shuffle_epi32(xout1, 0xAA); // see PSHUFD, set all elems to 3rd elem
    *xout2 = sl_xor(*xout2);
    *xout2 = _mm_xor_si128(*xout2, xout1);
}

#pragma GCC target ("sse4.2")
FLATTEN static inline void aes_genkey(const __m128i* memory, __m128i* k0, __m128i* k1, __m128i* k2, __m128i* k3,
    __m128i* k4, __m128i* k5, __m128i* k6, __m128i* k7, __m128i* k8, __m128i* k9)
{
    __m128i xout0, xout2;

    xout0 = _mm_load_si128(memory);
    xout2 = _mm_load_si128(memory+1);
    *k0 = xout0;
    *k1 = xout2;

    aes_genkey_sub<0x01>(&xout0, &xout2);
    *k2 = xout0;
    *k3 = xout2;

    aes_genkey_sub<0x02>(&xout0, &xout2);
    *k4 = xout0;
    *k5 = xout2;

    aes_genkey_sub<0x04>(&xout0, &xout2);
    *k6 = xout0;
    *k7 = xout2;

    aes_genkey_sub<0x08>(&xout0, &xout2);
    *k8 = xout0;
    *k9 = xout2;
}
#pragma GCC reset_options

FLATTEN static inline void soft_aes_genkey(const __m128i* memory, __m128i* k0, __m128i* k1, __m128i* k2, __m128i* k3,
    __m128i* k4, __m128i* k5, __m128i* k6, __m128i* k7, __m128i* k8, __m128i* k9)
{
    __m128i xout0, xout2;

    xout0 = _mm_load_si128(memory);
    xout2 = _mm_load_si128(memory+1);
    *k0 = xout0;
    *k1 = xout2;

    soft_aes_genkey_sub<0x01>(&xout0, &xout2);
    *k2 = xout0;
    *k3 = xout2;

    soft_aes_genkey_sub<0x02>(&xout0, &xout2);
    *k4 = xout0;
    *k5 = xout2;

    soft_aes_genkey_sub<0x04>(&xout0, &xout2);
    *k6 = xout0;
    *k7 = xout2;

    soft_aes_genkey_sub<0x08>(&xout0, &xout2);
    *k8 = xout0;
    *k9 = xout2;
}

#pragma GCC target ("sse4.2")
ALWAYS_INLINE FLATTEN static inline void aes_8round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6, __m128i* x7)
{
    *x0 = _mm_aesenc_si128(*x0, key);
    *x1 = _mm_aesenc_si128(*x1, key);
    *x2 = _mm_aesenc_si128(*x2, key);
    *x3 = _mm_aesenc_si128(*x3, key);
    *x4 = _mm_aesenc_si128(*x4, key);
    *x5 = _mm_aesenc_si128(*x5, key);
    *x6 = _mm_aesenc_si128(*x6, key);
    *x7 = _mm_aesenc_si128(*x7, key);
}
#pragma GCC reset_options

ALWAYS_INLINE FLATTEN static inline void soft_aes_8round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6, __m128i* x7)
{
    *x0 = soft_aesenc(*x0, key);
    *x1 = soft_aesenc(*x1, key);
    *x2 = soft_aesenc(*x2, key);
    *x3 = soft_aesenc(*x3, key);
    *x4 = soft_aesenc(*x4, key);
    *x5 = soft_aesenc(*x5, key);
    *x6 = soft_aesenc(*x6, key);
    *x7 = soft_aesenc(*x7, key);
}

ALWAYS_INLINE FLATTEN static inline void soft_aes_4round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3)
{
    *x0 = soft_aesenc(*x0, key);
    *x1 = soft_aesenc(*x1, key);
    *x2 = soft_aesenc(*x2, key);
    *x3 = soft_aesenc(*x3, key);
}

#pragma GCC target ("sse4.2")
template<size_t MEM, bool PREFETCH>
ALIGN(64) FLATTEN2 void cn_explode_scratchpad(const __m128i* input, __m128i* output)
{
    // This is more than we have registers, compiler will assign 2 keys on the stack
    __m128i xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    if(PREFETCH){
        _mm_prefetch((const char*)input + 0, _MM_HINT_T0);
        _mm_prefetch((const char*)input + 4, _MM_HINT_T0);
        _mm_prefetch((const char*)input + 8, _MM_HINT_T0);
    }

    aes_genkey(input, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xin0 = _mm_load_si128(input + 4);
    xin1 = _mm_load_si128(input + 5);
    xin2 = _mm_load_si128(input + 6);
    xin3 = _mm_load_si128(input + 7);
    xin4 = _mm_load_si128(input + 8);
    xin5 = _mm_load_si128(input + 9);
    xin6 = _mm_load_si128(input + 10);
    xin7 = _mm_load_si128(input + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
    {
        aes_8round(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_8round(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);

        if(PREFETCH){
            _mm_prefetch((const char*)output + i + 0, _MM_HINT_NTA);
            _mm_prefetch((const char*)output + i + 4, _MM_HINT_NTA);
        }

        _mm_store_si128(output + i + 0, xin0);
        _mm_store_si128(output + i + 1, xin1);
        _mm_store_si128(output + i + 2, xin2);
        _mm_store_si128(output + i + 3, xin3);

        if(PREFETCH)
            _mm_prefetch((const char*)output + i + 0, _MM_HINT_T2);

        _mm_store_si128(output + i + 4, xin4);
        _mm_store_si128(output + i + 5, xin5);
        _mm_store_si128(output + i + 6, xin6);
        _mm_store_si128(output + i + 7, xin7);

        if(PREFETCH)
            _mm_prefetch((const char*)output + i + 4, _MM_HINT_T2);
    }

}
#pragma GCC reset_options

template<size_t MEM, bool PREFETCH>
ALIGN(64) FLATTEN2 void soft_cn_explode_scratchpad(const __m128i* input, __m128i* output)
{
    // This is more than we have registers, compiler will assign 2 keys on the stack
    __m128i xin0, xin1, xin2, xin3;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    if(PREFETCH){
        _mm_prefetch((const char*)input + 0, _MM_HINT_T0);
        _mm_prefetch((const char*)input + 4, _MM_HINT_T0);
        _mm_prefetch((const char*)input + 8, _MM_HINT_T0);
    }

    soft_aes_genkey(input, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xin0 = _mm_load_si128(input + 4);
    xin1 = _mm_load_si128(input + 5);
    xin2 = _mm_load_si128(input + 6);
    xin3 = _mm_load_si128(input + 7);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
    {
        soft_aes_4round(k0, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k1, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k2, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k3, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k4, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k5, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k6, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k7, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k8, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k9, &xin0, &xin1, &xin2, &xin3);

        _mm_store_si128(output + i + 0, xin0);
        _mm_store_si128(output + i + 1, xin1);
        _mm_store_si128(output + i + 2, xin2);
        _mm_store_si128(output + i + 3, xin3);
    }

    xin0 = _mm_load_si128(input + 8);
    xin1 = _mm_load_si128(input + 9);
    xin2 = _mm_load_si128(input + 10);
    xin3 = _mm_load_si128(input + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
    {
        soft_aes_4round(k0, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k1, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k2, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k3, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k4, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k5, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k6, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k7, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k8, &xin0, &xin1, &xin2, &xin3);
        soft_aes_4round(k9, &xin0, &xin1, &xin2, &xin3);

        _mm_store_si128(output + i + 4, xin0);
        _mm_store_si128(output + i + 5, xin1);
        _mm_store_si128(output + i + 6, xin2);
        _mm_store_si128(output + i + 7, xin3);

        if(PREFETCH){
            _mm_prefetch((const char*)output + i + 0, _MM_HINT_T2);
            _mm_prefetch((const char*)output + i + 4, _MM_HINT_T2);
        }
    }
}

#pragma GCC target ("sse4.2")
template<size_t MEM, bool PREFETCH>
ALIGN(64) FLATTEN2 void cn_implode_scratchpad(const __m128i* input, __m128i* output)
{
    // This is more than we have registers, compiler will assign 2 keys on the stack
    __m128i xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    aes_genkey(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    if(PREFETCH){
        _mm_prefetch((const char*)input + 0, _MM_HINT_T0);
        _mm_prefetch((const char*)input + 4, _MM_HINT_T0);
    }

    xout0 = _mm_load_si128(output + 4);
    xout1 = _mm_load_si128(output + 5);
    xout2 = _mm_load_si128(output + 6);
    xout3 = _mm_load_si128(output + 7);
    xout4 = _mm_load_si128(output + 8);
    xout5 = _mm_load_si128(output + 9);
    xout6 = _mm_load_si128(output + 10);
    xout7 = _mm_load_si128(output + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
    {
        if(PREFETCH)
            _mm_prefetch((const char*)input + i + 8, _MM_HINT_T0);

        xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
        xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
        xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
        xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);

        if(PREFETCH)
            _mm_prefetch((const char*)input + i + 12, _MM_HINT_T0);

        xout4 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout4);
        xout5 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout5);
        xout6 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout6);
        xout7 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout7);

        aes_8round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_8round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
    }

    _mm_store_si128(output + 4, xout0);
    _mm_store_si128(output + 5, xout1);
    _mm_store_si128(output + 6, xout2);
    _mm_store_si128(output + 7, xout3);
    _mm_store_si128(output + 8, xout4);
    _mm_store_si128(output + 9, xout5);
    _mm_store_si128(output + 10, xout6);
    _mm_store_si128(output + 11, xout7);
}
#pragma GCC reset_options


template<size_t MEM, bool PREFETCH>
ALIGN(64) FLATTEN2 void soft_cn_implode_scratchpad(const __m128i* input, __m128i* output)
{
    // This is more than we have registers, compiler will assign 2 keys on the stack
    __m128i xout0, xout1, xout2, xout3;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    soft_aes_genkey(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    if(PREFETCH)
        _mm_prefetch((const char*)input + 0, _MM_HINT_T0);

    xout0 = _mm_load_si128(output + 4);
    xout1 = _mm_load_si128(output + 5);
    xout2 = _mm_load_si128(output + 6);
    xout3 = _mm_load_si128(output + 7);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
    {
        if(PREFETCH)
            _mm_prefetch((const char*)input + i + 8, _MM_HINT_T0);

        xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
        xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
        xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
        xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);

        soft_aes_4round(k0, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k1, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k2, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k3, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k4, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k5, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k6, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k7, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k8, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k9, &xout0, &xout1, &xout2, &xout3);
    }

    _mm_store_si128(output + 4, xout0);
    _mm_store_si128(output + 5, xout1);
    _mm_store_si128(output + 6, xout2);
    _mm_store_si128(output + 7, xout3);

    if(PREFETCH)
        _mm_prefetch((const char*)input + 4, _MM_HINT_T0);

    xout0 = _mm_load_si128(output + 8);
    xout1 = _mm_load_si128(output + 9);
    xout2 = _mm_load_si128(output + 10);
    xout3 = _mm_load_si128(output + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
    {
        if(PREFETCH)
            _mm_prefetch((const char*)input + i + 12, _MM_HINT_T0);

        xout0 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout0);
        xout1 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout1);
        xout2 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout2);
        xout3 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout3);

        soft_aes_4round(k0, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k1, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k2, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k3, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k4, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k5, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k6, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k7, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k8, &xout0, &xout1, &xout2, &xout3);
        soft_aes_4round(k9, &xout0, &xout1, &xout2, &xout3);
    }

    _mm_store_si128(output + 8, xout0);
    _mm_store_si128(output + 9, xout1);
    _mm_store_si128(output + 10, xout2);
    _mm_store_si128(output + 11, xout3);
}

template<size_t ITERATIONS, size_t MEM, bool SOFT_AES, bool PREFETCH>
TARGETS("avx2,avx,popcnt,fma,fma4,bmi,bmi2,xop,sse4.2,sse4.1,sse4a,ssse3,sse3,default")
OPTIMIZE("no-align-loops")
ALIGN(64) void cryptonight_hash(const void* input, size_t len, void* output, cryptonight_ctx* ctx0)
{
    keccak<200>((const uint8_t *)input, len, ctx0->hash_state);

    // Optim - 99% time boundary
    if(SOFT_AES)
        soft_cn_explode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->hash_state, (__m128i*)ctx0->long_state);
    else
        cn_explode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->hash_state, (__m128i*)ctx0->long_state);

    uint8_t* l0 = ctx0->long_state;
    uint64_t* h0 = (uint64_t*)ctx0->hash_state;

    uint64_t al0 = h0[0] ^ h0[4];
    uint64_t ah0 = h0[1] ^ h0[5];
    __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);

    uint64_t idx0 = h0[0] ^ h0[4];

    // Optim - 90% time boundary
    for(size_t i = 0; i < ITERATIONS; i++)
    {
        __m128i cx;
        cx = _mm_load_si128((__m128i *)&l0[idx0 & 0x1FFFF0]);

        if(SOFT_AES)
            cx = soft_aesenc(cx, _mm_set_epi64x(ah0, al0));
        else
            cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah0, al0));

        _mm_store_si128((__m128i *)&l0[idx0 & 0x1FFFF0], _mm_xor_si128(bx0, cx));
        uint64_t idx1 = _mm_cvtsi128_si64(cx);
        bx0 = cx;

        if(PREFETCH)
            _mm_prefetch((const char*)&l0[idx1 & 0x1FFFF0], _MM_HINT_T0);

        uint64_t hi, lo, cl, ch;
        cl = ((uint64_t*)&l0[idx1 & 0x1FFFF0])[0];
        ch = ((uint64_t*)&l0[idx1 & 0x1FFFF0])[1];

        lo = _umul128(idx1, cl, &hi);

        al0 += hi;
        ah0 += lo;
        ((uint64_t*)&l0[idx1 & 0x1FFFF0])[0] = al0;
        ((uint64_t*)&l0[idx1 & 0x1FFFF0])[1] = ah0;
        ah0 ^= ch;
        al0 ^= cl;
        idx0 = al0;

        if(PREFETCH)
            _mm_prefetch((const char*)&l0[idx0 & 0x1FFFF0], _MM_HINT_T0);
    }

    // Optim - 90% time boundary
    if(SOFT_AES)
        soft_cn_implode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->long_state, (__m128i*)ctx0->hash_state);
    else
        cn_implode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->long_state, (__m128i*)ctx0->hash_state);

    // Optim - 99% time boundary

    keccakf<24>((uint64_t*)ctx0->hash_state);
    extra_hashes[ctx0->hash_state[0] & 3](ctx0->hash_state, (char*)output);
}

// This lovely creation will do 2 cn hashes at a time. We have plenty of space on silicon
// to fit temporary vars for two contexts. Function will read len*2 from input and write 64 bytes to output
// We are still limited by L3 cache, so doubling will only work with CPUs where we have more than 2MB to core (Xeons)
template<size_t ITERATIONS, size_t MEM, bool SOFT_AES, bool PREFETCH>
TARGETS("avx2,avx,popcnt,fma,fma4,bmi,bmi2,xop,sse4.2,sse4.1,sse4a,ssse3,sse3,default")
OPTIMIZE("no-align-loops")
ALIGN(64) void cryptonight_double_hash(const void* input, size_t len, void* output, cryptonight_ctx* __restrict ctx0, cryptonight_ctx* __restrict ctx1)
{
    keccak<200>((const uint8_t *)input, len, ctx0->hash_state);
    keccak<200>((const uint8_t *)input+len, len, ctx1->hash_state);

    // Optim - 99% time boundary
    if(SOFT_AES){
        soft_cn_explode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->hash_state, (__m128i*)ctx0->long_state);
        soft_cn_explode_scratchpad<MEM, PREFETCH>((__m128i*)ctx1->hash_state, (__m128i*)ctx1->long_state);
    }else{
        cn_explode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->hash_state, (__m128i*)ctx0->long_state);
        cn_explode_scratchpad<MEM, PREFETCH>((__m128i*)ctx1->hash_state, (__m128i*)ctx1->long_state);
    }

    uint8_t* l0 = ctx0->long_state;
    uint64_t* h0 = (uint64_t*)ctx0->hash_state;
    uint8_t* l1 = ctx1->long_state;
    uint64_t* h1 = (uint64_t*)ctx1->hash_state;

    uint64_t axl0 = h0[0] ^ h0[4];
    uint64_t axh0 = h0[1] ^ h0[5];
    __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
    uint64_t axl1 = h1[0] ^ h1[4];
    uint64_t axh1 = h1[1] ^ h1[5];
    __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

    uint64_t idx0 = h0[0] ^ h0[4];
    uint64_t idx1 = h1[0] ^ h1[4];

    // Optim - 90% time boundary
    for (size_t i = 0; i < ITERATIONS; i++)
    {
        __m128i cx;
        cx = _mm_load_si128((__m128i *)&l0[idx0 & 0x1FFFF0]);

        if(SOFT_AES)
            cx = soft_aesenc(cx, _mm_set_epi64x(axh0, axl0));
        else
            cx = _mm_aesenc_si128(cx, _mm_set_epi64x(axh0, axl0));

        _mm_store_si128((__m128i *)&l0[idx0 & 0x1FFFF0], _mm_xor_si128(bx0, cx));
        idx0 = _mm_cvtsi128_si64(cx);
        bx0 = cx;

        if(PREFETCH)
            _mm_prefetch((const char*)&l0[idx0 & 0x1FFFF0], _MM_HINT_T0);

        cx = _mm_load_si128((__m128i *)&l1[idx1 & 0x1FFFF0]);

        if(SOFT_AES)
            cx = soft_aesenc(cx, _mm_set_epi64x(axh1, axl1));
        else
            cx = _mm_aesenc_si128(cx, _mm_set_epi64x(axh1, axl1));

        _mm_store_si128((__m128i *)&l1[idx1 & 0x1FFFF0], _mm_xor_si128(bx1, cx));
        idx1 = _mm_cvtsi128_si64(cx);
        bx1 = cx;

        if(PREFETCH)
            _mm_prefetch((const char*)&l1[idx1 & 0x1FFFF0], _MM_HINT_T0);

        uint64_t hi, lo, cl, ch;
        cl = ((uint64_t*)&l0[idx0 & 0x1FFFF0])[0];
        ch = ((uint64_t*)&l0[idx0 & 0x1FFFF0])[1];

        lo = _umul128(idx0, cl, &hi);

        axl0 += hi;
        axh0 += lo;
        ((uint64_t*)&l0[idx0 & 0x1FFFF0])[0] = axl0;
        ((uint64_t*)&l0[idx0 & 0x1FFFF0])[1] = axh0;
        axh0 ^= ch;
        axl0 ^= cl;
        idx0 = axl0;

        if(PREFETCH)
            _mm_prefetch((const char*)&l0[idx0 & 0x1FFFF0], _MM_HINT_T0);

        cl = ((uint64_t*)&l1[idx1 & 0x1FFFF0])[0];
        ch = ((uint64_t*)&l1[idx1 & 0x1FFFF0])[1];

        lo = _umul128(idx1, cl, &hi);

        axl1 += hi;
        axh1 += lo;
        ((uint64_t*)&l1[idx1 & 0x1FFFF0])[0] = axl1;
        ((uint64_t*)&l1[idx1 & 0x1FFFF0])[1] = axh1;
        axh1 ^= ch;
        axl1 ^= cl;
        idx1 = axl1;

        if(PREFETCH)
            _mm_prefetch((const char*)&l1[idx1 & 0x1FFFF0], _MM_HINT_T0);
    }

    // Optim - 90% time boundary
    if(SOFT_AES){
        soft_cn_implode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->long_state, (__m128i*)ctx0->hash_state);
        soft_cn_implode_scratchpad<MEM, PREFETCH>((__m128i*)ctx1->long_state, (__m128i*)ctx1->hash_state);
    }else{
        cn_implode_scratchpad<MEM, PREFETCH>((__m128i*)ctx0->long_state, (__m128i*)ctx0->hash_state);
        cn_implode_scratchpad<MEM, PREFETCH>((__m128i*)ctx1->long_state, (__m128i*)ctx1->hash_state);
    }

    // Optim - 99% time boundary

    keccakf<24>((uint64_t*)ctx0->hash_state);
    extra_hashes[ctx0->hash_state[0] & 3](ctx0->hash_state, (char*)output);
    keccakf<24>((uint64_t*)ctx1->hash_state);
    extra_hashes[ctx1->hash_state[0] & 3](ctx1->hash_state, (char*)output + 32);
}
