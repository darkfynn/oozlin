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

#include "kraken.h"
#include "utilities.h"
#include "kraken_bits.h"



// Log2RoundUp() XXX NOT USED
//int Log2RoundUp(uint32 v) {
//  if (v > 1) {
//    unsigned long idx;
//    //_BitScanReverse(&idx, v - 1);
//    idx = 32 - __builtin_clz(v - 1) - 1;
//    return idx + 1;
//  } else {
//    return 0;
//  }
//}


// Kraken_Create()
KrakenDecoder *Kraken_Create()
{
    size_t scratch_size = 0x6C000;
    size_t memory_needed = sizeof(KrakenDecoder) + scratch_size;
    KrakenDecoder *dec = (KrakenDecoder*)MallocAligned(memory_needed, 16);
    memset(dec, 0, sizeof(KrakenDecoder));
    dec->scratch_size = scratch_size;
    dec->scratch = (byte*)(dec + 1);
    return dec;
}



// Kraken_Destroy()
void Kraken_Destroy(KrakenDecoder *kraken)
{
    FreeAligned(kraken);
}



// Kraken_ParseHeader()
const byte *Kraken_ParseHeader(KrakenHeader *hdr, const byte *p)
{
    int b = p[0];

    if ((b & 0xF) == 0xC)
    {
        if (((b >> 4) & 3) != 0)
        {
            return NULL;
        }

        hdr->restart_decoder = (b >> 7) & 1;
        hdr->uncompressed = (b >> 6) & 1;
        b = p[1];
        hdr->decoder_type = b & 0x7F;
        hdr->use_checksums = !!(b >> 7);

        if (hdr->decoder_type != 6 && hdr->decoder_type != 10 && hdr->decoder_type != 5 && hdr->decoder_type != 11 && hdr->decoder_type != 12)
        {
            return NULL;
        }

        return p + 2;
    }

  return NULL;
}



// Kraken_ParseQuantumHeader()
const byte *Kraken_ParseQuantumHeader(KrakenQuantumHeader *hdr, const byte *p, bool use_checksum)
{
    uint32_t v = (p[0] << 16) | (p[1] << 8) | p[2];
    uint32_t size = v & 0x3FFFF;

    if (size != 0x3ffff)
    {
        hdr->compressed_size = size + 1;
        hdr->flag1 = (v >> 18) & 1;
        hdr->flag2 = (v >> 19) & 1;
        if (use_checksum)
        {
            hdr->checksum = (p[3] << 16) | (p[4] << 8) | p[5];
            return p + 6;
        }
        else
        {
            return p + 3;
        }
    }
    v >>= 18;
    if (v == 1)
    {
        // memset
        hdr->checksum = p[3];
        hdr->compressed_size = 0;
        hdr->whole_match_distance = 0;
        return p + 4;
    }
    return NULL;
}



// Kraken_GetCrc()
uint32_t Kraken_GetCrc(const byte *p, size_t p_size)
{
    // TODO: implement
    return 0;
}



// static
//
// Rearranges elements in the input array so that bits in the index
// get flipped.
static void ReverseBitsArray2048(const byte *input, byte *output)
{
    static const uint8_t offsets[32] = {
        0,    0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8
    };
    __m128i t0;
    __m128i t1;
    __m128i t2;
    __m128i t3;
    __m128i s0;
    __m128i s1;
    __m128i s2;
    __m128i s3;
    int i;
    int j;

    for(i = 0; i != 32; i++) {
        j = offsets[i];
        t0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&input[j]), 
                               _mm_loadl_epi64((const __m128i *)&input[j + 256]));
        t1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&input[j + 512]),
                               _mm_loadl_epi64((const __m128i *)&input[j + 768]));
        t2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&input[j + 1024]),
                               _mm_loadl_epi64((const __m128i *)&input[j + 1280]));
        t3 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&input[j + 1536]),
                               _mm_loadl_epi64((const __m128i *)&input[j + 1792]));

        s0 = _mm_unpacklo_epi8(t0, t1);
        s1 = _mm_unpacklo_epi8(t2, t3);
        s2 = _mm_unpackhi_epi8(t0, t1);
        s3 = _mm_unpackhi_epi8(t2, t3);

        t0 = _mm_unpacklo_epi8(s0, s1);
        t1 = _mm_unpacklo_epi8(s2, s3);
        t2 = _mm_unpackhi_epi8(s0, s1);
        t3 = _mm_unpackhi_epi8(s2, s3);

        _mm_storel_epi64((__m128i *)&output[0], t0);
        _mm_storeh_pi((__m64*)&output[1024], _mm_castsi128_ps(t0));
        _mm_storel_epi64((__m128i *)&output[256], t1);
        _mm_storeh_pi((__m64*)&output[1280], _mm_castsi128_ps(t1));
        _mm_storel_epi64((__m128i *)&output[512], t2);
        _mm_storeh_pi((__m64*)&output[1536], _mm_castsi128_ps(t2));
        _mm_storel_epi64((__m128i *)&output[768], t3);
        _mm_storeh_pi((__m64*)&output[1792], _mm_castsi128_ps(t3));
        output += 8;
    }
}



// Kraken_DecodeBytesCore()
bool Kraken_DecodeBytesCore(HuffReader *hr, HuffRevLut *lut)
{
    const byte *src = hr->src;
    uint32_t src_bits = hr->src_bits;
    int src_bitpos = hr->src_bitpos;

    const byte *src_mid = hr->src_mid;
    uint32_t src_mid_bits = hr->src_mid_bits;
    int src_mid_bitpos = hr->src_mid_bitpos;

    const byte *src_end = hr->src_end;
    uint32_t src_end_bits = hr->src_end_bits;
    int src_end_bitpos = hr->src_end_bitpos;

    int k;
    int n;

    byte *dst = hr->output;
    byte *dst_end = hr->output_end;

    if (src > src_mid)
    {
        return false;
    }

    if (hr->src_end - src_mid >= 4 && dst_end - dst >= 6)
    {
        dst_end -= 5;
        src_end -= 4;

        while (dst < dst_end && src <= src_mid && src_mid <= src_end)
        {
            src_bits |= *(uint32_t*)src << src_bitpos;
            src += (31 - src_bitpos) >> 3;

            src_end_bits |= bswap_32(*(uint32_t*)src_end) << src_end_bitpos;
            src_end -= (31 - src_end_bitpos) >> 3;

            src_mid_bits |= *(uint32_t*)src_mid << src_mid_bitpos;
            src_mid += (31 - src_mid_bitpos) >> 3;

            src_bitpos |= 0x18;
            src_end_bitpos |= 0x18;
            src_mid_bitpos |= 0x18;

            k = src_bits & 0x7FF;
            n = lut->bits2len[k];
            src_bits >>= n;
            src_bitpos -= n;
            dst[0] = lut->bits2sym[k];

            k = src_end_bits & 0x7FF;
            n = lut->bits2len[k];
            src_end_bits >>= n;
            src_end_bitpos -= n;
            dst[1] = lut->bits2sym[k];

            k = src_mid_bits & 0x7FF;
            n = lut->bits2len[k];
            src_mid_bits >>= n;
            src_mid_bitpos -= n;
            dst[2] = lut->bits2sym[k];

            k = src_bits & 0x7FF;
            n = lut->bits2len[k];
            src_bits >>= n;
            src_bitpos -= n;
            dst[3] = lut->bits2sym[k];

            k = src_end_bits & 0x7FF;
            n = lut->bits2len[k];
            src_end_bits >>= n;
            src_end_bitpos -= n;
            dst[4] = lut->bits2sym[k];

            k = src_mid_bits & 0x7FF;
            n = lut->bits2len[k];
            src_mid_bits >>= n;
            src_mid_bitpos -= n;
            dst[5] = lut->bits2sym[k];
            dst += 6;
        }
        dst_end += 5;

        src -= src_bitpos >> 3;
        src_bitpos &= 7;

        src_end += 4 + (src_end_bitpos >> 3);
        src_end_bitpos &= 7;

        src_mid -= src_mid_bitpos >> 3;
        src_mid_bitpos &= 7;
    }
    for(;;)
    {
        if (dst >= dst_end)
        {
            break;
        }

        if (src_mid - src <= 1)
        {
            if (src_mid - src == 1)
            {
                src_bits |= *src << src_bitpos;
            }
        }
        else
        {
            src_bits |= *(uint16_t *)src << src_bitpos;
        }

        k = src_bits & 0x7FF;
        n = lut->bits2len[k];
        src_bitpos -= n;
        src_bits >>= n;
        *dst++ = lut->bits2sym[k];
        src += (7 - src_bitpos) >> 3;
        src_bitpos &= 7;

        if (dst < dst_end)
        {
            if (src_end - src_mid <= 1)
            {
                if (src_end - src_mid == 1)
                {
                    src_end_bits |= *src_mid << src_end_bitpos;
                    src_mid_bits |= *src_mid << src_mid_bitpos;
                }
            }
            else
            {
                unsigned int v = *(uint16_t*)(src_end - 2);
                src_end_bits |= (((v >> 8) | (v << 8)) & 0xffff) << src_end_bitpos;
                src_mid_bits |= *(uint16_t*)src_mid << src_mid_bitpos;
            }
            n = lut->bits2len[src_end_bits & 0x7FF];
            *dst++ = lut->bits2sym[src_end_bits & 0x7FF];
            src_end_bitpos -= n;
            src_end_bits >>= n;
            src_end -= (7 - src_end_bitpos) >> 3;
            src_end_bitpos &= 7;
            if (dst < dst_end)
            {
                n = lut->bits2len[src_mid_bits & 0x7FF];
                *dst++ = lut->bits2sym[src_mid_bits & 0x7FF];
                src_mid_bitpos -= n;
                src_mid_bits >>= n;
                src_mid += (7 - src_mid_bitpos) >> 3;
                src_mid_bitpos &= 7;
            }
        }
        if (src > src_mid || src_mid > src_end)
        {
            return false;
        }
    }
    if (src != hr->src_mid_org || src_end != src_mid)
    {
        return false;
    }
    return true;
}




// Kraken_DecodeBytes_Type12()
int Kraken_DecodeBytes_Type12(const byte *src, size_t src_size, byte *output, int output_size, int type)
{
    BitReader bits;
    int half_output_size;
    uint32_t split_left;
    uint32_t split_mid;
    uint32_t split_right;
    const byte *src_mid;
    NewHuffLut huff_lut;
    HuffReader hr;
    HuffRevLut rev_lut;
    const uint8_t *src_end = src + src_size;

    bits.bitpos = 24;
    bits.bits = 0;
    bits.p = src;
    bits.p_end = src_end;
    BitReader_Refill(&bits);

    static const uint32_t code_prefix_org[12] = { 0x0, 0x0, 0x2, 0x6,
                                                  0xE, 0x1E, 0x3E, 0x7E,
                                                  0xFE, 0x1FE, 0x2FE, 0x3FE
                                                };
    uint32_t code_prefix[12] = { 0x0, 0x0, 0x2, 0x6, 0xE, 0x1E,
                                 0x3E, 0x7E, 0xFE, 0x1FE, 0x2FE, 0x3FE
                               };
    uint8_t syms[1280];
    int num_syms;

    if (!BitReader_ReadBitNoRefill(&bits))
    {
        num_syms = Huff_ReadCodeLengthsOld(&bits, syms, code_prefix);
    }
    else if (!BitReader_ReadBitNoRefill(&bits))
    {
        num_syms = Huff_ReadCodeLengthsNew(&bits, syms, code_prefix);
    }
    else
    {
        return -1;
    }

    if (num_syms < 1)
    {
        return -1;
    }
    src = bits.p - ((24 - bits.bitpos) / 8);

    if (num_syms == 1)
    {
        memset(output, syms[0], output_size);
        return src - src_end;
    }
  
    if (!Huff_MakeLut(code_prefix_org, code_prefix, &huff_lut, syms))
    {
        return -1;
    }

    ReverseBitsArray2048(huff_lut.bits2len, rev_lut.bits2len);
    ReverseBitsArray2048(huff_lut.bits2sym, rev_lut.bits2sym);

    if (type == 1)
    {
        if (src + 3 > src_end)
        {
            return -1;
        }
        split_mid = *(uint16_t*)src;
        src += 2;
        hr.output = output;
        hr.output_end = output + output_size;
        hr.src = src;
        hr.src_end = src_end;
        hr.src_mid_org = hr.src_mid = src + split_mid;
        hr.src_bitpos = 0;
        hr.src_bits = 0;
        hr.src_mid_bitpos = 0;
        hr.src_mid_bits = 0;
        hr.src_end_bitpos = 0;
        hr.src_end_bits = 0;
        if (!Kraken_DecodeBytesCore(&hr, &rev_lut))
        {
            return -1;
        }
    }
    else
    {
        if (src + 6 > src_end)
        {
            return -1;
        }

        half_output_size = (output_size + 1) >> 1;
        split_mid = *(uint32_t*)src & 0xFFFFFF;
        src += 3;
        if (split_mid > (src_end - src))
        {
            return -1;
        }
        src_mid = src + split_mid;
        split_left = *(uint16_t*)src;
        src += 2;
        if (src_mid - src < split_left + 2 || src_end - src_mid < 3)
        {
            return -1;
        }
        split_right = *(uint16_t*)src_mid;
        if (src_end - (src_mid + 2) < split_right + 2)
        {
            return -1;
        }

        hr.output = output;
        hr.output_end = output + half_output_size;
        hr.src = src;
        hr.src_end = src_mid;
        hr.src_mid_org = hr.src_mid = src + split_left;
        hr.src_bitpos = 0;
        hr.src_bits = 0;
        hr.src_mid_bitpos = 0;
        hr.src_mid_bits = 0;
        hr.src_end_bitpos = 0;
        hr.src_end_bits = 0;
        if (!Kraken_DecodeBytesCore(&hr, &rev_lut))
        {
            return -1;
        }

        hr.output = output + half_output_size;
        hr.output_end = output + output_size;
        hr.src = src_mid + 2;
        hr.src_end = src_end;
        hr.src_mid_org = hr.src_mid = src_mid + 2 + split_right;
        hr.src_bitpos = 0;
        hr.src_bits = 0;
        hr.src_mid_bitpos = 0;
        hr.src_mid_bits = 0;
        hr.src_end_bitpos = 0;
        hr.src_end_bits = 0;
        if (!Kraken_DecodeBytesCore(&hr, &rev_lut))
        {
            return -1;
        }
    }
    return (int)src_size;
}


// Kraken_DecodeMultiArray()
int Kraken_DecodeMultiArray(const uint8_t *src, const uint8_t *src_end,
                            uint8_t *dst, uint8_t *dst_end,
                            uint8_t **array_data, int *array_lens, int array_count,
                            int *total_size_out, bool force_memmove, uint8_t *scratch, uint8_t *scratch_end) {
    const uint8_t *src_org = src;

    if (src_end - src < 4)
    {
        return -1;
    }

    int decoded_size;
    int num_arrays_in_file = *src++;
    if (!(num_arrays_in_file & 0x80))
    {
        return -1;
    }
    num_arrays_in_file &= 0x3f;

    if (dst == scratch)
    {
        // todo: ensure scratch space first?
        scratch += (scratch_end - scratch - 0xc000) >> 1;
        dst_end = scratch;
    }

    int total_size = 0;

    if (num_arrays_in_file == 0)
    {
        for (int i = 0; i < array_count; i++)
        {
            uint8_t *chunk_dst = dst;
            int dec = Kraken_DecodeBytes(&chunk_dst, src, src_end, &decoded_size, dst_end - dst, force_memmove, scratch, scratch_end);
            if (dec < 0)
            {
                return -1;
            }
            dst += decoded_size;
            array_lens[i] = decoded_size;
            array_data[i] = chunk_dst;
            src += dec;
            total_size += decoded_size;
        }
        *total_size_out = total_size;
        return src - src_org; // not supported yet
    }

    uint8_t *entropy_array_data[32];
    uint32_t entropy_array_size[32];

    // First loop just decodes everything to scratch
    uint8_t *scratch_cur = scratch;

    for(int i = 0; i < num_arrays_in_file; i++)
    {
        uint8_t *chunk_dst = scratch_cur;
        int dec = Kraken_DecodeBytes(&chunk_dst, src, src_end, &decoded_size, scratch_end - scratch_cur, force_memmove, scratch_cur, scratch_end);
        if (dec < 0)
        {
            return -1;
        }
        entropy_array_data[i] = chunk_dst;
        entropy_array_size[i] = decoded_size;
        scratch_cur += decoded_size;
        total_size += decoded_size;
        src += dec;
    }
    *total_size_out = total_size;

    if (src_end - src < 3)
    {
        return -1;
    }
  
    int Q = *(uint16_t*)src;
    src += 2;

    int out_size;
    if (Kraken_GetBlockSize(src, src_end, &out_size, total_size) < 0)
    {
        return -1;
    }
    int num_indexes = out_size;

    int num_lens = num_indexes - array_count;
    if (num_lens < 1)
    {
        return -1;
    }

    if (scratch_end - scratch_cur < num_indexes)
    {
        return -1;
    }
    uint8_t *interval_lenlog2 = scratch_cur;
    scratch_cur += num_indexes;

    if (scratch_end - scratch_cur < num_indexes)
    {
        return -1;
    }
    uint8_t *interval_indexes = scratch_cur;
    scratch_cur += num_indexes;

 
    if (Q & 0x8000)
    {
        int size_out;
        int n = Kraken_DecodeBytes(&interval_indexes, src, src_end, &size_out, num_indexes, false, scratch_cur, scratch_end);
        if (n < 0 || size_out != num_indexes)
        {
            return -1;
        }
        src += n;

        for (int i = 0; i < num_indexes; i++) {
            int t = interval_indexes[i];
            interval_lenlog2[i] = t >> 4;
            interval_indexes[i] = t & 0xF;
        }

        num_lens = num_indexes;
    }
    else
    {
        int lenlog2_chunksize = num_indexes - array_count;

        int size_out;
        int n = Kraken_DecodeBytes(&interval_indexes, src, src_end, &size_out, num_indexes, false, scratch_cur, scratch_end);
        if (n < 0 || size_out != num_indexes)
        {
            return -1;
        }
        src += n;

        n = Kraken_DecodeBytes(&interval_lenlog2, src, src_end, &size_out, lenlog2_chunksize, false, scratch_cur, scratch_end);
        if (n < 0 || size_out != lenlog2_chunksize)
        {
            return -1;
        }
        src += n;

        for (int i = 0; i < lenlog2_chunksize; i++)
        {
            if (interval_lenlog2[i] > 16)
            {
                return -1;
            }
        }
    }

    if (scratch_end - scratch_cur < 4)
    {
        return -1;
    }

    scratch_cur = ALIGN_POINTER(scratch_cur, 4);
    if (scratch_end - scratch_cur < num_lens * 4)
    {
        return -1;
    }
    uint32_t *decoded_intervals = (uint32_t*)scratch_cur;

    int varbits_complen = Q & 0x3FFF;
    if (src_end - src < varbits_complen)
    {
        return -1;
    }

    const uint8_t *f = src;
    uint32_t bits_f = 0;
    int bitpos_f = 24;

    const uint8_t *src_end_actual = src + varbits_complen;

    const uint8_t *b = src_end_actual;
    uint32_t bits_b = 0;
    int bitpos_b = 24;
  
    int i;
    for (i = 0; i + 2 <= num_lens; i += 2)
    {
        bits_f |= bswap_32(*(uint32_t*)f) >> (24 - bitpos_f);
        f += (bitpos_f + 7) >> 3;

        bits_b |= ((uint32_t*)b)[-1] >> (24 - bitpos_b);
        b -= (bitpos_b + 7) >> 3;

        int numbits_f = interval_lenlog2[i + 0];
        int numbits_b = interval_lenlog2[i + 1];

        bits_f = _rotl(bits_f | 1, numbits_f);
        bitpos_f += numbits_f - 8 * ((bitpos_f + 7) >> 3);

        bits_b = _rotl(bits_b | 1, numbits_b);
        bitpos_b += numbits_b - 8 * ((bitpos_b + 7) >> 3);

        int value_f = bits_f & bitmasks[numbits_f];
        bits_f &= ~bitmasks[numbits_f];

        int value_b = bits_b & bitmasks[numbits_b];
        bits_b &= ~bitmasks[numbits_b];

        decoded_intervals[i + 0] = value_f;
        decoded_intervals[i + 1] = value_b;
    }

    // read final one since above loop reads 2
    if (i < num_lens)
    {
        bits_f |= bswap_32(*(uint32_t*)f) >> (24 - bitpos_f);
        int numbits_f = interval_lenlog2[i];
        bits_f = _rotl(bits_f | 1, numbits_f);
        int value_f = bits_f & bitmasks[numbits_f];
        decoded_intervals[i + 0] = value_f;
    }

    if (interval_indexes[num_indexes - 1])
    {
        return -1;
    }

    int indi = 0, leni = 0, source;
    int increment_leni = (Q & 0x8000) != 0;

    for(int arri = 0; arri < array_count; arri++)
    {
        array_data[arri] = dst;
        if (indi >= num_indexes)
        {
            return -1;
        }

        while ((source = interval_indexes[indi++]) != 0)
        {
            if (source > num_arrays_in_file)
            {
                return -1;
            }
            if (leni >= num_lens)
            {
                return -1;
            }
            int cur_len = decoded_intervals[leni++];
            int bytes_left = entropy_array_size[source - 1];
            if (cur_len > bytes_left || cur_len > dst_end - dst)
            {
                return -1;
            }
            uint8_t *blksrc = entropy_array_data[source - 1];
            entropy_array_size[source - 1] -= cur_len;
            entropy_array_data[source - 1] += cur_len;
            uint8_t *dstx = dst;
            dst += cur_len;
            memcpy(dstx, blksrc, cur_len);
        }
        leni += increment_leni;
        array_lens[arri] = dst - array_data[arri];
    }

    if (indi != num_indexes || leni != num_lens)
    {
        return -1;
    }

    for (int i = 0; i < num_arrays_in_file; i++)
    {
        if (entropy_array_size[i])
        {
            return -1;
        }
    }
    return src_end_actual - src_org;
}



// Krak_DecodeRecursive()
int Krak_DecodeRecursive(const byte *src, size_t src_size, byte *output, int output_size, uint8_t *scratch, uint8_t *scratch_end)
{
    const uint8_t *src_org = src;
    byte *output_end = output + output_size;
    const byte *src_end = src + src_size;

    if (src_size < 6)
    {
        return -1;
    }

    int n = src[0] & 0x7f;
    if (n < 2)
    {
        return -1;
    }

    if (!(src[0] & 0x80))
    {
        src++;
        do {
            int decoded_size;
            int dec = Kraken_DecodeBytes(&output, src, src_end, &decoded_size, output_end - output, true, scratch, scratch_end);
            if (dec < 0)
            {
                return -1;
            }
            output += decoded_size;
            src += dec;
        } while (--n);
        if (output != output_end)
        {
            return -1;
        }
        return src - src_org;
    }
    else
    {
        uint8_t *array_data;
        int array_len, decoded_size;
        int dec = Kraken_DecodeMultiArray(src, src_end, output, output_end, &array_data, &array_len, 1, &decoded_size, true, scratch, scratch_end);
        if (dec < 0)
        {
            return -1;
        }
        output += decoded_size;
        if (output != output_end)
        {
            return -1;
        }
        return dec;
    }
}


// Krak_DecodeRLE()
int Krak_DecodeRLE(const byte *src, size_t src_size, byte *dst, int dst_size, uint8_t *scratch, uint8_t *scratch_end)
{

    if (src_size <= 1)
    {
        if (src_size != 1)
        {
            return -1;
        }
        memset(dst, src[0], dst_size);
        return 1;
    }

    uint8_t *dst_end = dst + dst_size;
    const uint8_t *cmd_ptr = src + 1, *cmd_ptr_end = src + src_size;

    // Unpack the first X bytes of the command buffer?
    if (src[0])
    {
        uint8_t *dst_ptr = scratch;
        int dec_size;
        int n = Kraken_DecodeBytes(&dst_ptr, src, src + src_size, &dec_size, scratch_end - scratch, true, scratch, scratch_end);
        if (n <= 0)
        {
            return -1;
        }
        int cmd_len = src_size - n + dec_size;
        if (cmd_len > scratch_end - scratch)
        {
            return -1;
        }
        memcpy(dst_ptr + dec_size, src + n, src_size - n);
        cmd_ptr = dst_ptr;
        cmd_ptr_end = &dst_ptr[cmd_len];
    }

    int rle_byte = 0;

    while (cmd_ptr < cmd_ptr_end)
    {
        uint32_t cmd = cmd_ptr_end[-1];
        if (cmd - 1 >= 0x2f)
        {
            cmd_ptr_end--;
            uint32_t bytes_to_copy = (-1 - cmd) & 0xF;
            uint32_t bytes_to_rle = cmd >> 4;
            if (dst_end - dst < bytes_to_copy + bytes_to_rle || cmd_ptr_end - cmd_ptr < bytes_to_copy)
            {
                return -1;
            }
            memcpy(dst, cmd_ptr, bytes_to_copy);
            cmd_ptr += bytes_to_copy;
            dst += bytes_to_copy;
            memset(dst, rle_byte, bytes_to_rle);
            dst += bytes_to_rle;
        }
        else if (cmd >= 0x10)
        {
            uint32_t data = *(uint16_t*)(cmd_ptr_end - 2) - 4096;
            cmd_ptr_end -= 2;
            uint32_t bytes_to_copy = data & 0x3F;
            uint32_t bytes_to_rle = data >> 6;
            if (dst_end - dst < bytes_to_copy + bytes_to_rle || cmd_ptr_end - cmd_ptr < bytes_to_copy)
            {
                return -1;
            }
            memcpy(dst, cmd_ptr, bytes_to_copy);
            cmd_ptr += bytes_to_copy;
            dst += bytes_to_copy;
            memset(dst, rle_byte, bytes_to_rle);
            dst += bytes_to_rle;
        }
        else if (cmd == 1)
        {
            rle_byte = *cmd_ptr++;
            cmd_ptr_end--;
        }
        else if (cmd >= 9)
        {
            uint32_t bytes_to_rle = (*(uint16_t*)(cmd_ptr_end - 2) - 0x8ff) * 128;
            cmd_ptr_end -= 2;
            if (dst_end - dst < bytes_to_rle)
            {
                return -1;
            }
            memset(dst, rle_byte, bytes_to_rle);
            dst += bytes_to_rle;
        }
        else
        {
            uint32_t bytes_to_copy = (*(uint16_t*)(cmd_ptr_end - 2) - 511) * 64;
            cmd_ptr_end -= 2;
            if (cmd_ptr_end - cmd_ptr < bytes_to_copy || dst_end - dst < bytes_to_copy)
            {
                return -1;
            }
            memcpy(dst, cmd_ptr, bytes_to_copy);
            dst += bytes_to_copy;
            cmd_ptr += bytes_to_copy;
        }
    }

    if (cmd_ptr_end != cmd_ptr)
    {
        return -1;
    }

    if (dst != dst_end)
    {
        return -1;
    }

    return src_size;
}



// Tans_DecodeTable()
bool Tans_DecodeTable(BitReader *bits, int L_bits, TansData *tans_data)
{
    BitReader_Refill(bits);
    if (BitReader_ReadBitNoRefill(bits))
    {
        int Q = BitReader_ReadBitsNoRefill(bits, 3);
        int num_symbols = BitReader_ReadBitsNoRefill(bits, 8) + 1;

        if (num_symbols < 2)
        {
            return false;
        }

        int fluff = BitReader_ReadFluff(bits, num_symbols);
        int total_rice_values = fluff + num_symbols;
        uint8_t rice[512 + 16];
        BitReader2 br2;

        // another bit reader...
        br2.p = bits->p - ((uint)(24 - bits->bitpos + 7) >> 3);
        br2.p_end = bits->p_end;
        br2.bitpos = (bits->bitpos - 24) & 7;
    
        if (!DecodeGolombRiceLengths(rice, total_rice_values, &br2))
        {
            return false;
        }
        memset(rice + total_rice_values, 0, 16);

        // Switch back to other bitreader impl
        bits->bitpos = 24;
        bits->p = br2.p;
        bits->bits = 0;
        BitReader_Refill(bits);
        bits->bits <<= br2.bitpos;
        bits->bitpos += br2.bitpos;

        HuffRange range[133];
        fluff = Huff_ConvertToRanges(range, num_symbols, fluff, &rice[num_symbols], bits);
        if (fluff < 0)
        {
            return false;
        }

        BitReader_Refill(bits);

        uint32_t L = 1 << L_bits;
        uint8_t *cur_rice_ptr = rice;
        int average = 6;
        int somesum = 0;
        uint8_t *tanstable_A = tans_data->A;
        uint32_t *tanstable_B = tans_data->B;

        for (int ri = 0; ri < fluff; ri++)
        {
            int symbol = range[ri].symbol;
            int num = range[ri].num;
            do {
                BitReader_Refill(bits);
        
                int nextra = Q + *cur_rice_ptr++;
                if (nextra > 15)
                {
                    return false;
                }
                int v = BitReader_ReadBitsNoRefillZero(bits, nextra) + (1 << nextra) - (1 << Q);

                int average_div4 = average >> 2;
                int limit = 2 * average_div4;
                if (v <= limit)
                {
                    v = average_div4 + (-(v & 1) ^ ((uint32_t)v >> 1));
                }
                if (limit > v)
                {
                    limit = v;  
                }
                v += 1;
                average += limit - average_div4;
                *tanstable_A = symbol;
                *tanstable_B = (symbol << 16) + v;
                tanstable_A += (v == 1);
                tanstable_B += v >= 2;
                somesum += v;
                symbol += 1;
            } while (--num);
        }
        tans_data->A_used = tanstable_A - tans_data->A;
        tans_data->B_used = tanstable_B - tans_data->B;
        if (somesum != L)
        {
            return false;
        }

        return true;
    }
    else
    {
        bool seen[256];
        memset(seen, 0, sizeof(seen));
        uint32_t L = 1 << L_bits;

        tint count = BitReader_ReadBitsNoRefill(bits, 3) + 1;

        int bits_per_sym = BSR(L_bits) + 1;
        int max_delta_bits = BitReader_ReadBitsNoRefill(bits, bits_per_sym);

        if (max_delta_bits == 0 || max_delta_bits > L_bits)
        {
             return false;
        }

        uint8_t *tanstable_A = tans_data->A;
        uint32_t *tanstable_B = tans_data->B;

        int weight = 0;
        int total_weights = 0;

        do {
            BitReader_Refill(bits);

            int sym = BitReader_ReadBitsNoRefill(bits, 8);
            if (seen[sym])
            {
                return false;
            }
            int delta = BitReader_ReadBitsNoRefill(bits, max_delta_bits);

            weight += delta;

            if (weight == 0)
            {
                return false;
            }

            seen[sym] = true;
            if (weight == 1)
            {
                *tanstable_A++ = sym;
            }
            else
            {
                *tanstable_B++ = (sym << 16) + weight;
            }

            total_weights += weight;
        } while (--count);

        BitReader_Refill(bits);

        int sym = BitReader_ReadBitsNoRefill(bits, 8);
        if (seen[sym])
        {
            return false;
        }

        if (L - total_weights < weight || L - total_weights <= 1)
        {
            return false;
        }

        *tanstable_B++ = (sym << 16) + (L - total_weights);

        tans_data->A_used = tanstable_A - tans_data->A;
        tans_data->B_used = tanstable_B - tans_data->B;

        SimpleSort(tans_data->A, tanstable_A);
        SimpleSort(tans_data->B, tanstable_B);
        return true;
    }
}



// Tans_InitLut()
void Tans_InitLut(TansData *tans_data, int L_bits, TansLutEnt *lut)
{
    TansLutEnt *pointers[4];

    int L = 1 << L_bits;
    int a_used = tans_data->A_used;

    uint slots_left_to_alloc = L - a_used;

    uint sa = slots_left_to_alloc >> 2;
    pointers[0] = lut;
    uint sb = sa + ((slots_left_to_alloc & 3) > 0);
    pointers[1] = lut + sb;
    sb += sa + ((slots_left_to_alloc & 3) > 1);
    pointers[2] = lut + sb;
    sb += sa + ((slots_left_to_alloc & 3) > 2);
    pointers[3] = lut + sb;

    // Setup the single entrys with weight=1
    {
        TansLutEnt *lut_singles = lut + slots_left_to_alloc, le;
        le.w = 0;
        le.bits_x = L_bits;
        le.x = (1 << L_bits) - 1;
        for (int i = 0; i < a_used; i++)
        {
            lut_singles[i] = le;
            lut_singles[i].symbol = tans_data->A[i];
        }
    }

    // Setup the entrys with weight >= 2
    int weights_sum = 0;
    for (int i = 0; i < tans_data->B_used; i++)
    {
        int weight = tans_data->B[i] & 0xffff;
        int symbol = tans_data->B[i] >> 16;
        if (weight > 4)
        {
            uint32_t sym_bits = BSR(weight);
            int Z = L_bits - sym_bits;
            TansLutEnt le;
            le.symbol = symbol;
            le.bits_x = Z;
            le.x = (1 << Z) - 1;
            le.w = (L - 1) & (weight << Z);
            int what_to_add = 1 << Z;
            int X = (1 << (sym_bits + 1)) - weight;

            for (int j = 0; j < 4; j++)
            {
                TansLutEnt *dst = pointers[j];

                int Y = (weight + ((weights_sum - j - 1) & 3)) >> 2;
                if (X >= Y)
                {
                    for(int n = Y; n; n--)
                    {
                        *dst++ = le;
                        le.w += what_to_add;
                    }
                    X -= Y;
                }
                else
                {
                    for (int n = X; n; n--)
                    {
                        *dst++ = le;
                        le.w += what_to_add;
                    }
                    Z--;

                    what_to_add >>= 1;
                    le.bits_x = Z;
                    le.w = 0;
                    le.x >>= 1;
                    for (int n = Y - X; n; n--)
                    {
                        *dst++ = le;
                        le.w += what_to_add;
                    }
                    X = weight;
                }
                pointers[j] = dst;
            }
        }
        else
        {
            assert(weight > 0);
            uint32_t bits = ((1 << weight) - 1) << (weights_sum & 3);
            bits |= (bits >> 4);
            int n = weight, ww = weight;
            do {
                uint32_t idx = BSF(bits);
                bits &= bits - 1;
                TansLutEnt *dst = pointers[idx]++;
                dst->symbol = symbol;
                uint32_t weight_bits = BSR(ww);
                dst->bits_x = L_bits - weight_bits;
                dst->x = (1 << (L_bits - weight_bits)) - 1;
                dst->w = (L - 1) & (ww++ << (L_bits - weight_bits));
            } while (--n);
        }
        weights_sum += weight;
    }
}


// Tans_Decode()
bool Tans_Decode(TansDecoderParams *params)
{
    TansLutEnt *lut = params->lut, *e;
    uint8_t *dst = params->dst, *dst_end = params->dst_end;
    const uint8_t *ptr_f = params->ptr_f, *ptr_b = params->ptr_b;
    uint32_t bits_f = params->bits_f, bits_b = params->bits_b;
    int bitpos_f = params->bitpos_f, bitpos_b = params->bitpos_b;
    uint32_t state_0 = params->state_0, state_1 = params->state_1;
    uint32_t state_2 = params->state_2, state_3 = params->state_3;
    uint32_t state_4 = params->state_4;

    if (ptr_f > ptr_b)
    {
        return false;
    }

#define TANS_FORWARD_BITS()                     \
    bits_f |= *(uint32_t *)ptr_f << bitpos_f;     \
    ptr_f += (31 - bitpos_f) >> 3;              \
    bitpos_f |= 24;

#define TANS_FORWARD_ROUND(state)               \
    e = &lut[state];                            \
    *dst++ = e->symbol;                         \
    bitpos_f -= e->bits_x;                      \
    state = (bits_f & e->x) + e->w;             \
    bits_f >>= e->bits_x;                       \
    if (dst >= dst_end)                         \
        break;

#define TANS_BACKWARD_BITS()                    \
    bits_b |= bswap_32(((uint32_t *)ptr_b)[-1]) << bitpos_b;     \
    ptr_b -= (31 - bitpos_b) >> 3;              \
    bitpos_b |= 24;

#define TANS_BACKWARD_ROUND(state)              \
    e = &lut[state];                            \
    *dst++ = e->symbol;                         \
    bitpos_b -= e->bits_x;                      \
    state = (bits_b & e->x) + e->w;             \
    bits_b >>= e->bits_x;                       \
    if (dst >= dst_end)                         \
        break;
  
    if (dst < dst_end)
    {
        for (;;)
        {
            TANS_FORWARD_BITS();
            TANS_FORWARD_ROUND(state_0);
            TANS_FORWARD_ROUND(state_1);
            TANS_FORWARD_BITS();
            TANS_FORWARD_ROUND(state_2);
            TANS_FORWARD_ROUND(state_3);
            TANS_FORWARD_BITS();
            TANS_FORWARD_ROUND(state_4);
            TANS_BACKWARD_BITS();
            TANS_BACKWARD_ROUND(state_0);
            TANS_BACKWARD_ROUND(state_1);
            TANS_BACKWARD_BITS();
            TANS_BACKWARD_ROUND(state_2);
            TANS_BACKWARD_ROUND(state_3);
            TANS_BACKWARD_BITS();
            TANS_BACKWARD_ROUND(state_4);
        }
    }

    if (ptr_b - ptr_f + (bitpos_f >> 3) + (bitpos_b >> 3) != 0)
    {
        return false;
    }

    uint32_t states_or = state_0 | state_1 | state_2 | state_3 | state_4;
    if (states_or & ~0xFF)
    {
        return false;
    }

    dst_end[0] = (uint8_t)state_0;
    dst_end[1] = (uint8_t)state_1;
    dst_end[2] = (uint8_t)state_2;
    dst_end[3] = (uint8_t)state_3;
    dst_end[4] = (uint8_t)state_4;

    return true;
}



// Krak_DecodeTans()
int Krak_DecodeTans(const byte *src, size_t src_size, byte *dst, int dst_size, uint8_t *scratch, uint8_t *scratch_end)
{
    if (src_size < 8 || dst_size < 5)
    {
        return -1;
    }

    const uint8_t *src_end = src + src_size;

    BitReader br;
    TansData tans_data;

    br.bitpos = 24;
    br.bits = 0;
    br.p = src;
    br.p_end = src_end;
    BitReader_Refill(&br);

    // reserved bit
    if (BitReader_ReadBitNoRefill(&br))
    {
        return -1;
    }
  
    int L_bits = BitReader_ReadBitsNoRefill(&br, 2) + 8;

    if (!Tans_DecodeTable(&br, L_bits, &tans_data))
    {
        return -1;
    }

    src = br.p - (24 - br.bitpos) / 8;

    if (src >= src_end)
    {
        return -1;
    }

    uint32_t lut_space_required = ((sizeof(TansLutEnt) << L_bits) + 15) &~ 15;
    if (lut_space_required > (scratch_end - scratch))
    {
        return -1;
    }

    TansDecoderParams params;
    params.dst = dst;
    params.dst_end = dst + dst_size - 5;

    params.lut = (TansLutEnt *)ALIGN_POINTER(scratch, 16);
    Tans_InitLut(&tans_data, L_bits, params.lut);

    // Read out the initial state
    uint32_t L_mask = (1 << L_bits) - 1;
    uint32_t bits_f = *(uint32_t*)src;
    src += 4;
    uint32_t bits_b = bswap_32(*(uint32_t*)(src_end - 4));
    src_end -= 4;
    uint32_t bitpos_f = 32, bitpos_b = 32;

    // Read first two.
    params.state_0 = bits_f & L_mask;
    params.state_1 = bits_b & L_mask;
    bits_f >>= L_bits, bitpos_f -= L_bits;
    bits_b >>= L_bits, bitpos_b -= L_bits;

    // Read next two.
    params.state_2 = bits_f & L_mask;
    params.state_3 = bits_b & L_mask;
    bits_f >>= L_bits, bitpos_f -= L_bits;
    bits_b >>= L_bits, bitpos_b -= L_bits;

    // Refill more bits
    bits_f |= *(uint32_t *)src << bitpos_f;
    src += (31 - bitpos_f) >> 3;
    bitpos_f |= 24;

    // Read final state variable
    params.state_4 = bits_f & L_mask;
    bits_f >>= L_bits, bitpos_f -= L_bits;

    params.bits_f = bits_f;
    params.ptr_f = src - (bitpos_f >> 3);
    params.bitpos_f = bitpos_f & 7;

    params.bits_b = bits_b;
    params.ptr_b = src_end + (bitpos_b >> 3);
    params.bitpos_b = bitpos_b & 7;

    if (!Tans_Decode(&params))
    {
        return -1;
    }

    return src_size;
}



// Kraken_GetBlockSize()
int Kraken_GetBlockSize(const uint8_t *src, const uint8_t *src_end, int *dest_size, int dest_capacity)
{
    const byte *src_org = src;
    int src_size;
    int dst_size;

    if (src_end - src < 2)
    {
        return -1; // too few bytes
    }

    int chunk_type = (src[0] >> 4) & 0x7;
    if (chunk_type == 0)
    {
        if (src[0] >= 0x80)
        {
            // In this mode, memcopy stores the length in the bottom 12 bits.
            src_size = ((src[0] << 8) | src[1]) & 0xFFF;
            src += 2;
        }
        else
        {
            if (src_end - src < 3)
            {
                return -1; // too few bytes
            }
            src_size = ((src[0] << 16) | (src[1] << 8) | src[2]);
            if (src_size & ~0x3ffff)
            {
                return -1; // reserved bits must not be set
            }
            src += 3;
        }
        if (src_size > dest_capacity || src_end - src < src_size)
        {
            return -1;
        }
        *dest_size = src_size;
        return src + src_size - src_org;
    }

    if (chunk_type >= 6)
    {
        return -1;
    }

    // In all the other modes, the initial bytes encode
    // the src_size and the dst_size
    if (src[0] >= 0x80)
    {
        if (src_end - src < 3)
        {
            return -1; // too few bytes
        }
        // short mode, 10 bit sizes
        uint32_t bits = ((src[0] << 16) | (src[1] << 8) | src[2]);
        src_size = bits & 0x3ff;
        dst_size = src_size + ((bits >> 10) & 0x3ff) + 1;
        src += 3;
    }
    else
    {
        // long mode, 18 bit sizes
        if (src_end - src < 5)
        {
            return -1; // too few bytes
        }
        uint32_t bits = ((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
        src_size = bits & 0x3ffff;
        dst_size = (((bits >> 18) | (src[0] << 14)) & 0x3FFFF) + 1;
        if (src_size >= dst_size)
        {
            return -1;
        }
        src += 5;
    }
    if (src_end - src < src_size || dst_size > dest_capacity)
    {
        return -1;
    }
    *dest_size = dst_size;
    return src_size;
}



// Kraken_DecodeBytes()
int Kraken_DecodeBytes(byte **output, const byte *src, const byte *src_end,
                       int *decoded_size, size_t output_size, bool force_memmove,
                       uint8_t *scratch, uint8_t *scratch_end)
{
    const byte *src_org = src;
    int src_size;
    int dst_size;

    if (src_end - src < 2)
    {
        return -1; // too few bytes
    }

    int chunk_type = (src[0] >> 4) & 0x7;
    if (chunk_type == 0)
    {
        if (src[0] >= 0x80)
        {
            // In this mode, memcopy stores the length in the bottom 12 bits.
            src_size = ((src[0] << 8) | src[1]) & 0xFFF;
            src += 2;
        }
        else
        {
            if (src_end - src < 3)
            {
                return -1; // too few bytes
            }
            src_size = ((src[0] << 16) | (src[1] << 8) | src[2]);
            if (src_size & ~0x3ffff)
            {
                return -1; // reserved bits must not be set
            }
            src += 3;
        }
        if (src_size > output_size || src_end - src < src_size)
        {
            return -1;
        }
        *decoded_size = src_size;
        if (force_memmove)
        {
            memmove(*output, src, src_size);
        }
        else
        {
            *output = (byte*)src;
        }
        return src + src_size - src_org;
    }

    // In all the other modes, the initial bytes encode
    // the src_size and the dst_size
    if (src[0] >= 0x80)
    {
        if (src_end - src < 3)
        {
            return -1; // too few bytes
        }

        // short mode, 10 bit sizes
        uint32_t bits = ((src[0] << 16) | (src[1] << 8) | src[2]);
        src_size = bits & 0x3ff;
        dst_size = src_size + ((bits >> 10) & 0x3ff) + 1;
        src += 3;
    }
    else
    {
        // long mode, 18 bit sizes
        if (src_end - src < 5)
        {
            return -1; // too few bytes
        }
        uint32_t bits = ((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
        src_size = bits & 0x3ffff;
        dst_size = (((bits >> 18) | (src[0] << 14)) & 0x3FFFF) + 1;
        if (src_size >= dst_size)
        {
            return -1;
        }
        src += 5;
    }
    if (src_end - src < src_size || dst_size > output_size)
    {
        return -1;
    }

    uint8_t *dst = *output;
    if (dst == scratch)
    {
        if (scratch_end - scratch < dst_size)
        {
            return -1;
        }
        scratch += dst_size;
    }

    //  printf("%d -> %d (%d)\n", src_size, dst_size, chunk_type);

    int src_used = -1;
    switch (chunk_type) {
        case 2:
        case 4:
            src_used = Kraken_DecodeBytes_Type12(src, src_size, dst, dst_size, chunk_type >> 1);
            break;
        case 5:
            src_used = Krak_DecodeRecursive(src, src_size, dst, dst_size, scratch, scratch_end);
            break;
        case 3:
            src_used = Krak_DecodeRLE(src, src_size, dst, dst_size, scratch, scratch_end);
            break;
        case 1:
            src_used = Krak_DecodeTans(src, src_size, dst, dst_size, scratch, scratch_end);
            break;
    }
    if (src_used != src_size)
    {
        return -1;
    }
    *decoded_size = dst_size;
    return src + src_size - src_org;
}



// CombineScaledOffsetArrays()
void CombineScaledOffsetArrays(int *offs_stream, size_t offs_stream_size,
                               int scale, const uint8_t *low_bits)
{
    for (size_t i = 0; i != offs_stream_size; i++)
    {
        offs_stream[i] = scale * offs_stream[i] - low_bits[i];
    }
}



// Kraken_UnpackOffsets()
//
// Unpacks the packed 8 bit offset and lengths into 32 bit.
bool Kraken_UnpackOffsets(const byte *src, const byte *src_end,
                          const byte *packed_offs_stream, const byte *packed_offs_stream_extra, int packed_offs_stream_size,
                          int multi_dist_scale,
                          const byte *packed_litlen_stream, int packed_litlen_stream_size,
                          int *offs_stream, int *len_stream,
                          bool excess_flag, int excess_bytes) {


    BitReader bits_a;
    BitReader bits_b;
    int n;
    int i;
    int u32_len_stream_size = 0;
  
    bits_a.bitpos = 24;
    bits_a.bits = 0;
    bits_a.p = src;
    bits_a.p_end = src_end;
    BitReader_Refill(&bits_a);

    bits_b.bitpos = 24;
    bits_b.bits = 0;
    bits_b.p = src_end;
    bits_b.p_end = src;
    BitReader_RefillBackwards(&bits_b);

    if (!excess_flag)
    {
        if (bits_b.bits < 0x2000)
        {
            return false;
        }
        n = 31 - BSR(bits_b.bits);
        bits_b.bitpos += n;
        bits_b.bits <<= n;
        BitReader_RefillBackwards(&bits_b);
        n++;
        u32_len_stream_size = (bits_b.bits >> (32 - n)) - 1;
        bits_b.bitpos += n;
        bits_b.bits <<= n;
        BitReader_RefillBackwards(&bits_b);
    }
  
    if (multi_dist_scale == 0)
    {
        // Traditional way of coding offsets
        const uint8_t *packed_offs_stream_end = packed_offs_stream + packed_offs_stream_size;
        while (packed_offs_stream != packed_offs_stream_end)
        {
            *offs_stream++ = -(int32_t)BitReader_ReadDistance(&bits_a, *packed_offs_stream++);
            if (packed_offs_stream == packed_offs_stream_end)
            {
                break;
            }
            *offs_stream++ = -(int32_t)BitReader_ReadDistanceB(&bits_b, *packed_offs_stream++);
        }
    }
    else
    {
        // New way of coding offsets 
        int *offs_stream_org = offs_stream;
        const uint8_t *packed_offs_stream_end = packed_offs_stream + packed_offs_stream_size;
        uint32_t cmd;
        uint32_t offs;
        while (packed_offs_stream != packed_offs_stream_end)
        {
            cmd = *packed_offs_stream++;
            if ((cmd >> 3) > 26)
            {
                return 0;
            }
            offs = ((8 + (cmd & 7)) << (cmd >> 3)) | BitReader_ReadMoreThan24Bits(&bits_a, (cmd >> 3));
            *offs_stream++ = 8 - (int32_t)offs;
            if (packed_offs_stream == packed_offs_stream_end)
            {
                break;
            }
            cmd = *packed_offs_stream++;
            if ((cmd >> 3) > 26)
            {
                return 0;
            }
            offs = ((8 + (cmd & 7)) << (cmd >> 3)) | BitReader_ReadMoreThan24BitsB(&bits_b, (cmd >> 3));
            *offs_stream++ = 8 - (int32_t)offs;
        }
        if (multi_dist_scale != 1)
        {
            CombineScaledOffsetArrays(offs_stream_org, offs_stream - offs_stream_org, multi_dist_scale, packed_offs_stream_extra);
        }
    }
    uint32_t u32_len_stream_buf[512]; // max count is 128kb / 256 = 512
    if (u32_len_stream_size > 512)
    {
        return false;
    }
   
    uint32_t *u32_len_stream = u32_len_stream_buf;
    uint32_t *u32_len_stream_end = u32_len_stream_buf + u32_len_stream_size;
    for (i = 0; i + 1 < u32_len_stream_size; i += 2)
    {
        if (!BitReader_ReadLength(&bits_a, &u32_len_stream[i + 0]))
        {
            return false;
        }
        if (!BitReader_ReadLengthB(&bits_b, &u32_len_stream[i + 1]))
        {
            return false;
        }
    }
    if (i < u32_len_stream_size)
    {
        if (!BitReader_ReadLength(&bits_a, &u32_len_stream[i + 0]))
        {
            return false;
        }
    }

    bits_a.p -= (24 - bits_a.bitpos) >> 3;
    bits_b.p += (24 - bits_b.bitpos) >> 3;

    if (bits_a.p != bits_b.p)
    {
        return false;
    }

    for (i = 0; i < packed_litlen_stream_size; i++)
    {
        uint32_t v = packed_litlen_stream[i];
        if (v == 255)
        {
            v = *u32_len_stream++ + 255;
        }
        len_stream[i] = v + 3;
    }
    if (u32_len_stream != u32_len_stream_end)
    {
        return false;
    }

    return true;
}



// Kraken_ReadLzTable()
bool Kraken_ReadLzTable(int mode,
                        const byte *src, const byte *src_end,
                        byte *dst, int dst_size, int offset,
                        byte *scratch, byte *scratch_end, KrakenLzTable *lztable)
{
    byte *out;
    int decode_count, n;
    byte *packed_offs_stream;
    byte *packed_len_stream;

    if (mode > 1)
    {
        return false;
    }

    if (src_end - src < 13)
    {
        return false;
    }

    if (offset == 0)
    {
        COPY_64(dst, src);
        dst += 8;
        src += 8;
    }

    if (*src & 0x80)
    {
        uint8_t flag = *src++;
        if ((flag & 0xc0) != 0x80)
        {
            return false; // reserved flag set
        }
        return false; // excess bytes not supported
    }

    // Disable no copy optimization if source and dest overlap
    bool force_copy = dst <= src_end && src <= dst + dst_size;

    // Decode lit stream, bounded by dst_size
    out = scratch;
    n = Kraken_DecodeBytes(&out, src, src_end, &decode_count,
                           Min(scratch_end - scratch, dst_size),
                           force_copy, scratch, scratch_end);
    if (n < 0)
    {
        return false;
    }
    src += n;
    lztable->lit_stream = out;
    lztable->lit_stream_size = decode_count;
    scratch += decode_count;

    // Decode command stream, bounded by dst_size
    out = scratch;
    n = Kraken_DecodeBytes(&out, src, src_end, &decode_count,
                           Min(scratch_end - scratch, dst_size),
                           force_copy, scratch, scratch_end);
    if (n < 0)
    {
        return false;
    }
    src += n;
    lztable->cmd_stream = out;
    lztable->cmd_stream_size = decode_count;
    scratch += decode_count;

    // Check if to decode the multistuff crap
    if (src_end - src < 3)
    {
        return false;
    }

    int offs_scaling = 0;
    uint8_t *packed_offs_stream_extra = NULL;

    if (src[0] & 0x80)
    {
        // uses the mode where distances are coded with 2 tables
        offs_scaling = src[0] - 127;
        src++;

        packed_offs_stream = scratch;
        n = Kraken_DecodeBytes(&packed_offs_stream, src, src_end,
                               &lztable->offs_stream_size, Min(scratch_end - scratch,
                               lztable->cmd_stream_size), false, scratch, scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
        scratch += lztable->offs_stream_size;

        if (offs_scaling != 1)
        {
            packed_offs_stream_extra = scratch;
            n = Kraken_DecodeBytes(&packed_offs_stream_extra, src, src_end,
                                   &decode_count, Min(scratch_end - scratch,
                                   lztable->offs_stream_size), false, scratch,
                                   scratch_end);
            if (n < 0 || decode_count != lztable->offs_stream_size)
            {
                return false;
            }
            src += n;
            scratch += decode_count;
        }
    }
    else
    {
        // Decode packed offset stream, it's bounded by the command length.
        packed_offs_stream = scratch;
        n = Kraken_DecodeBytes(&packed_offs_stream, src, src_end,
                               &lztable->offs_stream_size, Min(scratch_end - scratch,
                               lztable->cmd_stream_size), false, scratch, scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
        scratch += lztable->offs_stream_size;
    }

    // Decode packed litlen stream. It's bounded by 1/4 of dst_size.
    packed_len_stream = scratch;
    n = Kraken_DecodeBytes(&packed_len_stream, src, src_end, &lztable->len_stream_size,
                           Min(scratch_end - scratch, dst_size >> 2), false,
                           scratch, scratch_end);
    if (n < 0)
    {
        return false;
    }
    src += n;
    scratch += lztable->len_stream_size;

    // Reserve memory for final dist stream
    scratch = ALIGN_POINTER(scratch, 16);
    lztable->offs_stream = (int*)scratch;
    scratch += lztable->offs_stream_size * 4;

    // Reserve memory for final len stream
    scratch = ALIGN_POINTER(scratch, 16);
    lztable->len_stream = (int*)scratch;
    scratch += lztable->len_stream_size * 4;

    if (scratch + 64 > scratch_end)
    {
        return false;
    }

    return Kraken_UnpackOffsets(src, src_end, packed_offs_stream, packed_offs_stream_extra,
                                lztable->offs_stream_size, offs_scaling,
                                packed_len_stream, lztable->len_stream_size,
                                lztable->offs_stream, lztable->len_stream, 0, 0);
}



// Kraken_ProcessLzRuns_Type0()
//
// Note: may access memory out of bounds on invalid input.
bool Kraken_ProcessLzRuns_Type0(KrakenLzTable *lzt, byte *dst, byte *dst_end, byte *dst_start)
{
    const byte *cmd_stream = lzt->cmd_stream;
    const byte *cmd_stream_end = cmd_stream + lzt->cmd_stream_size;
    const int *len_stream = lzt->len_stream;
    const int *len_stream_end = lzt->len_stream + lzt->len_stream_size;
    const byte *lit_stream = lzt->lit_stream;
    const byte *lit_stream_end = lzt->lit_stream + lzt->lit_stream_size;
    const int *offs_stream = lzt->offs_stream;
    const int *offs_stream_end = lzt->offs_stream + lzt->offs_stream_size;
    const byte *copyfrom;
    uint32_t final_len;
    int32_t offset;
    int32_t recent_offs[7];
    int32_t last_offset;

    recent_offs[3] = -8;
    recent_offs[4] = -8;
    recent_offs[5] = -8;
    last_offset = -8;

    while (cmd_stream < cmd_stream_end)
    {
        uint32_t f = *cmd_stream++;
        uint32_t litlen = f & 3;
        uint32_t offs_index = f >> 6;
        uint32_t matchlen = (f >> 2) & 0xF;

        // use cmov
        uint32_t next_long_length = *len_stream;
        const int *next_len_stream = len_stream + 1;

        len_stream = (litlen == 3) ? next_len_stream : len_stream;
        litlen = (litlen == 3) ? next_long_length : litlen;
        recent_offs[6] = *offs_stream;

        COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
        if (litlen > 8)
        {
            COPY_64_ADD(dst + 8, lit_stream + 8, &dst[last_offset + 8]);
            if (litlen > 16)
            {
                COPY_64_ADD(dst + 16, lit_stream + 16, &dst[last_offset + 16]);
                if (litlen > 24)
                {
                    do {
                        COPY_64_ADD(dst + 24, lit_stream + 24, &dst[last_offset + 24]);
                        litlen -= 8;
                        dst += 8;
                        lit_stream += 8;
                    } while (litlen > 24);
                }
            }
        }
        dst += litlen;
        lit_stream += litlen;

        offset = recent_offs[offs_index + 3];
        recent_offs[offs_index + 3] = recent_offs[offs_index + 2];
        recent_offs[offs_index + 2] = recent_offs[offs_index + 1];
        recent_offs[offs_index + 1] = recent_offs[offs_index + 0];
        recent_offs[3] = offset;
        last_offset = offset;

        offs_stream = (int*)((intptr_t)offs_stream + ((offs_index + 1) & 4));

        if ((uintptr_t)offset < (uintptr_t)(dst_start - dst))
        {
            return false; // offset out of bounds
        }

        copyfrom = dst + offset;
        if (matchlen != 15)
        {
            COPY_64(dst, copyfrom);
            COPY_64(dst + 8, copyfrom + 8);
            dst += matchlen + 2;
        }
        else
        {
            matchlen = 14 + *len_stream++; // why is the value not 16 here, the above case copies up to 16 bytes.
            if ((uintptr_t)matchlen >(uintptr_t)(dst_end - dst))
            {
                return false; // copy length out of bounds
            }
            COPY_64(dst, copyfrom);
            COPY_64(dst + 8, copyfrom + 8);
            COPY_64(dst + 16, copyfrom + 16);
            do {
                COPY_64(dst + 24, copyfrom + 24);
                matchlen -= 8;
                dst += 8;
                copyfrom += 8;
            } while (matchlen > 24);
            dst += matchlen;
        }
    }

    // check for incorrect input
    if (offs_stream != offs_stream_end || len_stream != len_stream_end)
    {
        return false;
    }

    final_len = dst_end - dst;
    if (final_len != lit_stream_end - lit_stream)
    {
        return false;
    }

    if (final_len >= 8)
    {
        do {
            COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
            dst += 8, lit_stream += 8, final_len -= 8;
        } while (final_len >= 8);
    }
    if (final_len > 0)
    {
        do {
            *dst = *lit_stream++ + dst[last_offset];
        } while (dst++, --final_len);
    }
    return true;
}



// Kraken_ProcessLzRuns_Type1()
//
// Note: may access memory out of bounds on invalid input.
bool Kraken_ProcessLzRuns_Type1(KrakenLzTable *lzt, byte *dst, byte *dst_end, byte *dst_start)
{
    const byte *cmd_stream = lzt->cmd_stream; 
    const byte *cmd_stream_end = cmd_stream + lzt->cmd_stream_size;
    const int *len_stream = lzt->len_stream;
    const int *len_stream_end = lzt->len_stream + lzt->len_stream_size;
    const byte *lit_stream = lzt->lit_stream;
    const byte *lit_stream_end = lzt->lit_stream + lzt->lit_stream_size;
    const int *offs_stream = lzt->offs_stream;
    const int *offs_stream_end = lzt->offs_stream + lzt->offs_stream_size;
    const byte *copyfrom;
    uint32_t final_len;
    int32_t offset;
    int32_t recent_offs[7];

    recent_offs[3] = -8;
    recent_offs[4] = -8;
    recent_offs[5] = -8;

    while (cmd_stream < cmd_stream_end)
    {
        uint32_t f = *cmd_stream++;
        uint32_t litlen = f & 3;
        uint32_t offs_index = f >> 6;
        uint32_t matchlen = (f >> 2) & 0xF;
  
        // use cmov
        uint32_t next_long_length = *len_stream;
        const int *next_len_stream = len_stream + 1;

        len_stream = (litlen == 3) ? next_len_stream : len_stream; 
        litlen = (litlen == 3) ? next_long_length : litlen;
        recent_offs[6] = *offs_stream;

        COPY_64(dst, lit_stream);
        if (litlen > 8)
        {
            COPY_64(dst + 8, lit_stream + 8);
            if (litlen > 16)
            {
                COPY_64(dst + 16, lit_stream + 16);
                if (litlen > 24)
                {
                    do {
                        COPY_64(dst + 24, lit_stream + 24);
                        litlen -= 8;
                        dst += 8;
                        lit_stream += 8;
                    } while (litlen > 24);
                }
            }
        }
        dst += litlen;
        lit_stream += litlen;

        offset = recent_offs[offs_index + 3];
        recent_offs[offs_index + 3] = recent_offs[offs_index + 2];
        recent_offs[offs_index + 2] = recent_offs[offs_index + 1];
        recent_offs[offs_index + 1] = recent_offs[offs_index + 0];
        recent_offs[3] = offset;
    
        offs_stream = (int*)((intptr_t)offs_stream + ((offs_index + 1) & 4));

        if ((uintptr_t)offset < (uintptr_t)(dst_start - dst))
        {
            return false; // offset out of bounds
        }

        copyfrom = dst + offset;
        if (matchlen != 15)
        {
            COPY_64(dst, copyfrom);
            COPY_64(dst + 8, copyfrom + 8);
            dst += matchlen + 2;
        }
        else
        {
            matchlen = 14 + *len_stream++; // why is the value not 16 here, the above case copies up to 16 bytes.
            if ((uintptr_t)matchlen > (uintptr_t)(dst_end - dst))
            {
                return false; // copy length out of bounds
            }
            COPY_64(dst, copyfrom);
            COPY_64(dst + 8, copyfrom + 8);
            COPY_64(dst + 16, copyfrom + 16);
            do {
                COPY_64(dst + 24, copyfrom + 24);
                matchlen -= 8;
                dst += 8;
                copyfrom += 8;
            } while (matchlen > 24);
            dst += matchlen;
        }
    }

    // check for incorrect input
    if (offs_stream != offs_stream_end || len_stream != len_stream_end)
    {
        return false;
    }

    final_len = dst_end - dst;
    if (final_len != lit_stream_end - lit_stream)
    {
        return false;
    }

    if (final_len >= 64)
    {
        do {
            COPY_64_BYTES(dst, lit_stream);
            dst += 64, lit_stream += 64, final_len -= 64;
        } while (final_len >= 64);
    }
    if (final_len >= 8)
    {
        do {
            COPY_64(dst, lit_stream);
            dst += 8, lit_stream += 8, final_len -= 8;
        } while (final_len >= 8);
    }
    if (final_len > 0)
    {
        do {
            *dst++ = *lit_stream++;
        } while (--final_len);
    }
    return true;
}



// Kraken_ProcessLzRuns()
bool Kraken_ProcessLzRuns(int mode, byte *dst, int dst_size, int offset, KrakenLzTable *lztable)
{
    byte *dst_end = dst + dst_size;

    if (mode == 1)
    {
        return Kraken_ProcessLzRuns_Type1(lztable, dst + (offset == 0 ? 8 : 0), dst_end, dst - offset);
    }

    if (mode == 0)
    {
        return Kraken_ProcessLzRuns_Type0(lztable, dst + (offset == 0 ? 8 : 0), dst_end, dst - offset);
    }

    return false;
}



// Kraken_DecodeQuantum()
//
// Decode one 256kb big quantum block. It's divided into two 128k blocks
// internally that are compressed separately but with a shared history.
int Kraken_DecodeQuantum(byte *dst, byte *dst_end, byte *dst_start,
                         const byte *src, const byte *src_end,
                         byte *scratch, byte *scratch_end)
{
    const byte *src_in = src;
    int mode;
    int chunkhdr;
    int dst_count;
    int src_used;
    int written_bytes;

    while (dst_end - dst != 0)
    {
        dst_count = dst_end - dst;
        if (dst_count > 0x20000)
        {
            dst_count = 0x20000;
        }
        if (src_end - src < 4)
        {
            return -1;
        }
        chunkhdr = src[2] | src[1] << 8 | src[0] << 16;
        if (!(chunkhdr & 0x800000))
        {
            // Stored as entropy without any match copying.
            byte *out = dst;
            src_used = Kraken_DecodeBytes(&out, src, src_end, &written_bytes, dst_count, false, scratch, scratch_end);
            if (src_used < 0 || written_bytes != dst_count)
            {
                return -1;
            }
        }
        else
        {
            src += 3;
            src_used = chunkhdr & 0x7FFFF;
            mode = (chunkhdr >> 19) & 0xF;
            if (src_end - src < src_used)
            {
                return -1;
            }
            if (src_used < dst_count)
            {
                size_t scratch_usage = Min(Min(3 * dst_count + 32 + 0xd000, 0x6C000), scratch_end - scratch);
                if (scratch_usage < sizeof(KrakenLzTable))
                {
                    return -1;
                }
                if (!Kraken_ReadLzTable(mode, src, src + src_used, dst, dst_count,
                                        dst - dst_start, scratch + sizeof(KrakenLzTable),
                                        scratch + scratch_usage, (KrakenLzTable*)scratch))
                {
                    return -1;
                }
                if (!Kraken_ProcessLzRuns(mode, dst, dst_count, dst - dst_start, (KrakenLzTable*)scratch))
                {
                    return -1;
                }
            }
            else if (src_used > dst_count || mode != 0)
            {
                return -1;
            }
            else
            {
                memmove(dst, src, dst_count);
            }
        }
        src += src_used;
        dst += dst_count;
    }
    return src - src_in;
}



// Kraken_CopyWholeMatch()
void Kraken_CopyWholeMatch(byte *dst, uint32_t offset, size_t length)
{
    size_t i = 0;
    byte *src = dst - offset;

    if (offset >= 8)
    {
        for (; i + 8 <= length; i += 8)
        {
            *(uint64_t*)(dst + i) = *(uint64_t*)(src + i);
        }
    } 
    for (; i < length; i++)
    {
        dst[i] = src[i];
    }
}



// Kraken_DecodeStep()
bool Kraken_DecodeStep(struct KrakenDecoder *dec, byte *dst_start, int offset,
                       size_t dst_bytes_left_in, const byte *src, size_t src_bytes_left)
{
    const byte *src_in = src;
    const byte *src_end = src + src_bytes_left;
    KrakenQuantumHeader qhdr;
    int n;

    if ((offset & 0x3FFFF) == 0)
    {
        src = Kraken_ParseHeader(&dec->hdr, src);
        if (!src)
        {
            return false;
        }
    }

    bool is_kraken_decoder = (dec->hdr.decoder_type == 6 || dec->hdr.decoder_type == 10 || dec->hdr.decoder_type == 12);

    int dst_bytes_left = (int)Min(is_kraken_decoder ? 0x40000 : 0x4000, dst_bytes_left_in);

    if (dec->hdr.uncompressed)
    {
        if (src_end - src < dst_bytes_left)
        {
            dec->src_used = dec->dst_used = 0;
             return true;
        }
        memmove(dst_start + offset, src, dst_bytes_left);
        dec->src_used = (src - src_in) + dst_bytes_left;
        dec->dst_used = dst_bytes_left;
        return true;
    }

    if (is_kraken_decoder)
    {
        src = Kraken_ParseQuantumHeader(&qhdr, src, dec->hdr.use_checksums);
    }
    else
    {
        src = LZNA_ParseQuantumHeader(&qhdr, src, dec->hdr.use_checksums, dst_bytes_left);
    }

    if (!src || src > src_end)
    {
        return false;
    }

    // Too few bytes in buffer to make any progress?
    if ((uintptr_t)(src_end - src) < qhdr.compressed_size)
    {
        dec->src_used = dec->dst_used = 0;
        return true;
    }
  
    if (qhdr.compressed_size > (uint32_t)dst_bytes_left)
    {
        return false;
    }

    if (qhdr.compressed_size == 0)
    {
        if (qhdr.whole_match_distance != 0)
        {
            if (qhdr.whole_match_distance > (uint32_t)offset)
            {
                return false;
            }
            Kraken_CopyWholeMatch(dst_start + offset, qhdr.whole_match_distance, dst_bytes_left);
        }
        else
        {
            memset(dst_start + offset, qhdr.checksum, dst_bytes_left);
        }
        dec->src_used = (src - src_in);
        dec->dst_used = dst_bytes_left;
        return true;
    }

    if (dec->hdr.use_checksums &&
       (Kraken_GetCrc(src, qhdr.compressed_size) & 0xFFFFFF) != qhdr.checksum)
    {
        return false;
    }

    if (qhdr.compressed_size == dst_bytes_left)
    {
        memmove(dst_start + offset, src, dst_bytes_left);
        dec->src_used = (src - src_in) + dst_bytes_left;
        dec->dst_used = dst_bytes_left;
        return true;
    }

    if (dec->hdr.decoder_type == 6)
    {
        n = Kraken_DecodeQuantum(dst_start + offset, dst_start + offset + dst_bytes_left,
                                 dst_start, src, src + qhdr.compressed_size,
                                 dec->scratch, dec->scratch + dec->scratch_size);
    }
    else if (dec->hdr.decoder_type == 5)
    {
        if (dec->hdr.restart_decoder)
        {
            dec->hdr.restart_decoder = false;
            LZNA_InitLookup((struct LznaState*)dec->scratch);
        }
        n = LZNA_DecodeQuantum(dst_start + offset, dst_start + offset + dst_bytes_left,
                               dst_start, src, src + qhdr.compressed_size,
                               (struct LznaState*)dec->scratch);
    }
    else if (dec->hdr.decoder_type == 11)
    {
        if (dec->hdr.restart_decoder)
        {
            dec->hdr.restart_decoder = false;
            BitknitState_Init((struct BitknitState*)dec->scratch);
        }
        n = (int)Bitknit_Decode(src, src + qhdr.compressed_size, dst_start + offset,
                                dst_start + offset + dst_bytes_left, dst_start,
                                (struct BitknitState*)dec->scratch);

    }
    else if (dec->hdr.decoder_type == 10)
    {
        n = Mermaid_DecodeQuantum(dst_start + offset, dst_start + offset + dst_bytes_left,
                                  dst_start, src, src + qhdr.compressed_size,
                                  dec->scratch, dec->scratch + dec->scratch_size);
    }
    else if (dec->hdr.decoder_type == 12)
    {
        n = Leviathan_DecodeQuantum(dst_start + offset, dst_start + offset + dst_bytes_left,
                                    dst_start, src, src + qhdr.compressed_size,
                                    dec->scratch, dec->scratch + dec->scratch_size);
    }
    else
    {
        return false;
    }

    if (n != qhdr.compressed_size)
    {
        return false;
    }

    dec->src_used = (src - src_in) + n;
    dec->dst_used = dst_bytes_left;
    return true;
}
  


// Kraken_Decompress()
int Kraken_Decompress(const byte *src, size_t src_len, byte *dst, size_t dst_len)
{
    KrakenDecoder *dec = Kraken_Create();
    int offset = 0;

    while (dst_len != 0)
    {
        if (!Kraken_DecodeStep(dec, dst, offset, dst_len, src, src_len))
        {
            goto FAIL;
        }
        if (dec->src_used == 0)
        {
            goto FAIL;
        }
        src += dec->src_used;
        src_len -= dec->src_used;
        dst_len -= dec->dst_used;
        offset += dec->dst_used;
    }
    if (src_len != 0)
    {
        goto FAIL;
    }
    Kraken_Destroy(dec);
    return offset;
FAIL:
    Kraken_Destroy(dec);
    return -1;
}

