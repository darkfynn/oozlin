// stdafx.h : includefile for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <byteswap.h>
#include <cstdint>
#include <dlfcn.h>

#define __forceinline __attribute__((always_inline)) inline

#define COPY_64(d, s) {*(uint64_t*)(d) = *(uint64_t*)(s); }
#define COPY_64_BYTES(d, s) {                                                 \
        _mm_storeu_si128((__m128i*)d + 0, _mm_loadu_si128((__m128i*)s + 0));  \
        _mm_storeu_si128((__m128i*)d + 1, _mm_loadu_si128((__m128i*)s + 1));  \
        _mm_storeu_si128((__m128i*)d + 2, _mm_loadu_si128((__m128i*)s + 2));  \
        _mm_storeu_si128((__m128i*)d + 3, _mm_loadu_si128((__m128i*)s + 3));  \
}
#define COPY_64_ADD(d, s, t) _mm_storel_epi64((__m128i *)(d), _mm_add_epi8(_mm_loadl_epi64((__m128i *)(s)), _mm_loadl_epi64((__m128i *)(t))))

// include file for x86 SSE2 types 
// compile with -msse2 (instructions available)
#ifdef __SSE2__
#include <emmintrin.h>
#else
#warning SSE2 support is not available.
#endif



//typedef int OodLZ_CompressFunc(int codec, uint8_t *src_buf, size_t src_len,
//                               uint8_t *dst_buf, int level, void *opts,
//                               size_t offs, size_t unused, void *scratch,
//                               size_t scratch_size);

//typedef int OodLZ_DecompressFunc(uint8_t *src_buf, int src_len, uint8_t *dst,
//                                 size_t dst_size, int fuzz, int crc, int verbose,
//                                 uint8_t *dst_base, size_t e, void *cb, void *cb_ctx,
//                                 void *scratch, size_t scratch_size, int threadPhase);


typedef size_t (*OodleLZ_CompressFunc)(int codec, uint8_t *src_buf, size_t src_len,
                                       uint8_t *dst_buf, int level, void *opts,
                                       size_t offs, size_t unused, void *scratch,
                                       size_t scratch_size
                                      );
typedef size_t (*OodleLZ_DecompressFunc)(uint8_t* srcBuf, size_t srcLen,
                                       uint8_t* dstBuf, size_t dstLen,
                                       int64_t unk1, int64_t unk2, int64_t unk3,
                                       int64_t unk4, int64_t unk5, int64_t unk6,
                                       int64_t unk7, int64_t unk8, int64_t unk9,
                                       int64_t unk10
                                      );



// Linux typedefs
typedef __uint8_t byte;
typedef __uint8_t uint8;
typedef __uint32_t uint32;
typedef __uint64_t uint64;
typedef __int64_t int64;
typedef __int32_t int32;
typedef __uint16_t uint16;
typedef __int16_t int16;


// LARGE_INTEGER union which mimics MS' LARGE_INTEGER union
typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int64_t LONGLONG;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    };
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER;


struct HuffRevLut {
    uint8 bits2len[2048];
    uint8 bits2sym[2048];
};


typedef struct HuffReader {

    // Array to hold the output of the huffman read array operation
    byte *output;
    byte *output_end;

    // We decode three parallel streams, two forwards, |src| and |src_mid|
    // while |src_end| is decoded backwards.
    const byte *src;
    const byte *src_mid;
    const byte *src_end;
    const byte *src_mid_org;
    int src_bitpos;
    int src_mid_bitpos;
    int src_end_bitpos;
    uint32_t src_bits;
    uint32_t src_mid_bits;
    uint32_t src_end_bits;

} HuffReader;


typedef struct BitReader {

    // |p| holds the current byte and |p_end| the end of the buffer.
    const byte *p;
    const byte *p_end;

    // Bits accumulated so far
    uint32_t bits;

    // Next byte will end up in the |bitpos| position in |bits|.
    int bitpos;

} BitReader;


struct BitReader2 {
    const uint8_t *p;
    const uint8_t *p_end;
    uint32_t bitpos;
};



// Header in front of each 256k block
typedef struct KrakenHeader {
    // Type of decoder used, 6 means kraken
    int decoder_type;

    // Whether to restart the decoder
    bool restart_decoder;

    // Whether this block is uncompressed
    bool uncompressed;

    // Whether this block uses checksums.
    bool use_checksums;
} KrakenHeader;


// Additional header in front of each 256k block ("quantum").
typedef struct KrakenQuantumHeader {
    // The compressed size of this quantum. If this value is 0 it means
    // the quantum is a special quantum such as memset.
    uint32_t compressed_size;
    // If checksums are enabled, holds the checksum.
    uint32_t checksum;
    // Two flags
    uint8_t flag1;
    uint8_t flag2;
    // Whether the whole block matched a previous block
    uint32_t whole_match_distance;
} KrakenQuantumHeader;

