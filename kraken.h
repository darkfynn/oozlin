/*
------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
------------------------------------------------------------------------------
*/

#include "stdafx.h"

#define ALIGN_POINTER(p, align) ((uint8*)(((uintptr_t)(p) + (align - 1)) & ~(align - 1)))
#define ALIGN_16(x) (((x)+15)&~15)

#if defined(_M_X64)
#define LIBNAME "oo2ext_7_win64.dll"
#else
#define LIBNAME "oo2ext_7_win32.dll"
#endif


// static var
static uint32 bitmasks[32] = {
    0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff,
    0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff,
    0x1ffff, 0x3ffff, 0x7ffff, 0xfffff, 0x1fffff, 0x3fffff, 0x7fffff,
    0xffffff, 0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
};



inline size_t Max(size_t a, size_t b) { return a > b ? a : b; }
inline size_t Min(size_t a, size_t b) { return a < b ? a : b; }




// template<>
template<typename T> void SimpleSort(T *p, T *pend) {
    if (p != pend)
    {   
        for (T *lp = p + 1, *rp; lp != pend; lp++)
        {
            T t = lp[0];
            for (rp = lp; rp > p && t < rp[-1]; rp--)
            {
                rp[0] = rp[-1];
            }   
            rp[0] = t;
        }   
    }   
}



//typedef size_t (*OodleLZ_CompressFunc)(int codec, uint8_t *src_buf, size_t src_len,
//                                       uint8_t *dst_buf, int level, void *opts,
//                                       size_t offs, size_t unused, void *scratch,
//                                       size_t scratch_size
//                                      );

//typedef size_t (*OodleLZ_DecompressFunc)(uint8_t* srcBuf, size_t srcLen,
//                                       uint8_t* dstBuf, size_t dstLen,
//                                       int64_t unk1, int64_t unk2, int64_t unk3,
//                                       int64_t unk4, int64_t unk5, int64_t unk6,
//                                       int64_t unk7, int64_t unk8, int64_t unk9,
//                                       int64_t unk10
//                                      );


struct TansData {
    uint32_t A_used;
    uint32_t B_used;
    uint8_t A[256];
    uint32_t B[256];
};



struct TansLutEnt {
    uint32_t x;
    uint8_t bits_x;
    uint8_t symbol;
    uint16_t w;
};


struct TansDecoderParams {
  TansLutEnt *lut;
  uint8_t *dst;
  uint8_t *dst_end;
  const uint8_t *ptr_f;
  const uint8_t *ptr_b;
  uint32_t bits_f;
  uint32_t bits_b;
  int bitpos_f;
  int bitpos_b;
  uint32_t state_0;
  uint32_t state_1;
  uint32_t state_2; 
  uint32_t state_3;
  uint32_t state_4;
};



// Kraken decompression happens in two phases, first one decodes
// all the literals and copy lengths using huffman and second
// phase runs the copy loop. This holds the tables needed by stage 2.
typedef struct KrakenLzTable {
    // Stream of (literal, match) pairs. The flag byte contains
    // the length of the match, the length of the literal and whether
    // to use a recent offset.
    uint8_t *cmd_stream;
    int cmd_stream_size;

    // Holds the actual distances in case we're not using a recent
    // offset.
    int *offs_stream;
    int offs_stream_size;

    // Holds the sequence of literals. All literal copying happens from
    // here.
    uint8_t *lit_stream;
    int lit_stream_size;

    // Holds the lengths that do not fit in the flag stream. Both literal
    // lengths and match length are stored in the same array.
    int *len_stream;
    int len_stream_size;
} KrakenLzTable;


typedef struct KrakenDecoder {
    // Updated after the |*_DecodeStep| function completes to hold
    // the number of bytes read and written.
    int src_used;
    int dst_used;

    // Pointer to a 256k buffer that holds the intermediate state
    // in between decode phase 1 and 2.
    uint8_t *scratch;
    size_t scratch_size;

    KrakenHeader hdr;
} KrakenDecoder;



// Prototypes
KrakenDecoder *Kraken_Create();
void Kraken_Destroy(KrakenDecoder *kraken);
const byte *Kraken_ParseHeader(KrakenHeader *hdr, const byte *p);
const byte *Kraken_ParseQuantumHeader(KrakenQuantumHeader *hdr, const byte *p, bool use_checksum);
uint32_t Kraken_GetCrc(const byte *p, size_t p_size);
static void ReverseBitsArray2048(const byte *input, byte *output);
bool Kraken_DecodeBytesCore(HuffReader *hr, HuffRevLut *lut);
int Kraken_DecodeBytes_Type12(const byte *src, size_t src_size, byte *output, int output_size, int type);
int Kraken_DecodeMultiArray(const uint8_t *src, const uint8_t *src_end,
                            uint8_t *dst, uint8_t *dst_end,
                            uint8_t **array_data, int *array_lens, int array_count,
                            int *total_size_out, bool force_memmove, uint8_t *scratch, uint8_t *scratch_end);
int Krak_DecodeRecursive(const byte *src, size_t src_size, byte *output, int output_size, uint8_t *scratch, uint8_t *scratch_end);
int Krak_DecodeRLE(const byte *src, size_t src_size, byte *dst, int dst_size, uint8_t *scratch, uint8_t *scratch_end);
bool Tans_DecodeTable(BitReader *bits, int L_bits, TansData *tans_data);
void Tans_InitLut(TansData *tans_data, int L_bits, TansLutEnt *lut);
bool Tans_Decode(TansDecoderParams *params);
int Krak_DecodeTans(const byte *src, size_t src_size, byte *dst, int dst_size, uint8_t *scratch, uint8_t *scratch_end);
int Kraken_GetBlockSize(const uint8_t *src, const uint8_t *src_end, int *dest_size, int dest_capacity);
int Kraken_DecodeBytes(byte **output, const byte *src, const byte *src_end,
                       int *decoded_size, size_t output_size, bool force_memmove,
                       uint8 *scratch, uint8 *scratch_end);
void CombineScaledOffsetArrays(int *offs_stream, size_t offs_stream_size,
                               int scale, const uint8_t *low_bits);
bool Kraken_UnpackOffsets(const byte *src, const byte *src_end,
                          const byte *packed_offs_stream, const byte *packed_offs_stream_extra, int packed_offs_stream_size,
                          int multi_dist_scale,
                          const byte *packed_litlen_stream, int packed_litlen_stream_size,
                          int *offs_stream, int *len_stream,
                          bool excess_flag, int excess_bytes);
bool Kraken_ReadLzTable(int mode,
                        const byte *src, const byte *src_end,
                        byte *dst, int dst_size, int offset,
                        byte *scratch, byte *scratch_end, KrakenLzTable *lztable);
bool Kraken_ProcessLzRuns_Type0(KrakenLzTable *lzt, byte *dst, byte *dst_end, byte *dst_start);
bool Kraken_ProcessLzRuns_Type1(KrakenLzTable *lzt, byte *dst, byte *dst_end, byte *dst_start);
bool Kraken_ProcessLzRuns(int mode, byte *dst, int dst_size, int offset, KrakenLzTable *lztable);
int Kraken_DecodeQuantum(byte *dst, byte *dst_end, byte *dst_start,
                         const byte *src, const byte *src_end,
                         byte *scratch, byte *scratch_end);
void Kraken_CopyWholeMatch(byte *dst, uint32_t offset, size_t length);
bool Kraken_DecodeStep(struct KrakenDecoder *dec, byte *dst_start, int offset,
                       size_t dst_bytes_left_in, const byte *src, size_t src_bytes_left);
int Kraken_Decompress(const byte *src, size_t src_len, byte *dst, size_t dst_len);

