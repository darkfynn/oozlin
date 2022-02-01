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
#include "lzna.h"



static LznaNibbleModel lzna_initializer_4bit = {
    0x0, 0x800, 0x1000, 0x1800, 0x2000, 0x2800, 0x3000, 0x3800, 0x4000, 0x4800,
    0x5000, 0x5800, 0x6000, 0x6800, 0x7000, 0x7800, 0x8000,
};



static Lzna3bitModel lzna_initializer_3bit = {
    0x0, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000
};



static void LznaNibbleModel_Init(LznaNibbleModel *d)
{
    *d = lzna_initializer_4bit;
}



static void Lzna3bitModel_Init(Lzna3bitModel *d)
{
    *d = lzna_initializer_3bit;
}



static void LznaNibbleModel_InitN(LznaNibbleModel *d, int n)
{
    do LznaNibbleModel_Init(d++); while (--n);
}



static void LznaLiteralModel_InitN(LznaLiteralModel *d, int n)
{
    do {
        LznaNibbleModel_InitN(d->upper, 16);
        LznaNibbleModel_InitN(d->lower, 16);
        LznaNibbleModel_InitN(d->nomatch, 16);
    } while (d++, --n);
}



static void LznaShortLengthRecentModel_InitN(LznaShortLengthRecentModel *d, int n)
{
    do {
        Lzna3bitModel_Init(&d->a[0]);
        Lzna3bitModel_Init(&d->a[1]);
        Lzna3bitModel_Init(&d->a[2]);
        Lzna3bitModel_Init(&d->a[3]);
    } while (d++, --n);
}



static void LznaNearDistModel_Init(LznaNearDistModel *d, int n)
{
    int i;

    do {
        LznaNibbleModel_Init(&d->first);

        for (i = 0; i < 16; i++)
        {
            d->second[i] = 0x2000;
            d->third[0][i] = 0x2000;
            d->third[1][i] = 0x2000;
        }
    } while (d++, --n);
}



static void LznaLowBitsDistanceModel_Init(LznaLowBitsDistanceModel *d, int n)
{
    do {
        d->v = 0x2000;
        LznaNibbleModel_InitN(d->d, 2);
    } while (d++, --n);
}



static void LznaFarDistModel_Init(LznaFarDistModel *d)
{
    int i;

    LznaNibbleModel_Init(&d->first_lo);
    LznaNibbleModel_Init(&d->first_hi);
    for (i = 0; i < 31; i++)
    {
        d->second[i] = 0x2000;
        d->third[0][i] = 0x2000;
        d->third[1][i] = 0x2000;
    }
}



void LZNA_InitLookup(LznaState *lut)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        lut->match_history[i + 4] = 1;
    }

    for (i = 0; i < 96; i++)
    {
        lut->is_literal[i] = 0x1000;
    }

    LznaNibbleModel_InitN(lut->type, 96);
    LznaLiteralModel_InitN(lut->literal, 4);
    LznaShortLengthRecentModel_InitN(lut->short_length_recent, 4);

    LznaNibbleModel_InitN(lut->long_length_recent.first, 4);
    LznaNibbleModel_Init(&lut->long_length_recent.second);
    LznaNibbleModel_InitN(&lut->long_length_recent.third, 1);

    for (i = 0; i < 48; i++)
    {
        lut->short_length[0][i] = 0x2000;
    }

    LznaNearDistModel_Init(lut->near_dist, 2);
    LznaLowBitsDistanceModel_Init(lut->low_bits_of_distance, 2);
  
    Lzna3bitModel_Init(&lut->medium_length);

    LznaNibbleModel_InitN(lut->long_length.first, 4);  
    LznaNibbleModel_Init(&lut->long_length.second);
    LznaNibbleModel_InitN(&lut->long_length.third, 1);
    LznaFarDistModel_Init(&lut->far_distance);
}



// Initialize bit reader with 2 parallel streams. Every decode operation
// swaps the two streams.
static void LznaBitReader_Init(LznaBitReader *tab, const byte *src)
{
    int d;
    int n;
    int i;
    uint64_t v;
  
    tab->src_start = (uint32_t*)src;

    d = *src++;
    n = d >> 4;
    assert(n <= 8);
    for (i = 0, v = 0; i < n; i++)
    {
        v = (v << 8) | *src++;
    }
    tab->bits_a = (v << 4) | (d & 0xF);

    d = *src++;
    n = d >> 4;
    assert(n <= 8);
    for (i = 0, v = 0; i < n; i++)
    {
        v = (v << 8) | *src++;
    }
    tab->bits_b = (v << 4) | (d & 0xF);
    tab->src = (uint32_t*)src;
}



// Renormalize by filling up the RANS state and swapping the two streams
static void __forceinline LznaRenormalize(LznaBitReader *tab)
{
    uint64_t x = tab->bits_a;

    if (x < 0x80000000)
    {
        x = (x << 32) | *tab->src++;
    }

    tab->bits_a = tab->bits_b;
    tab->bits_b = x;
}



// Read a single bit with a uniform distribution.
static uint32_t __forceinline LznaReadBit(LznaBitReader *tab)
{
    int r = tab->bits_a & 1;
    tab->bits_a >>= 1;
    LznaRenormalize(tab);
    return r;
}



// Read a number of bits with a uniform distribution.
static uint32_t __forceinline LznaReadNBits(LznaBitReader *tab, int bits)
{
    uint32_t rv = tab->bits_a & ((1 << bits) - 1);
    tab->bits_a >>= bits;
    LznaRenormalize(tab);
    return rv;
}



// Read a 4-bit value using an adaptive RANS model
static uint32_t __forceinline LznaReadNibble(LznaBitReader *tab, LznaNibbleModel *model)
{
    __m128i t;
    __m128i t0;
    __m128i t1;
    __m128i c0;
    __m128i c1;
    unsigned long bitindex;
    unsigned int start;
    unsigned int end;
    uint64_t x = tab->bits_a;

    t0 = _mm_loadu_si128((const __m128i *)&model->prob[0]);
    t1 = _mm_loadu_si128((const __m128i *)&model->prob[8]);

    t = _mm_cvtsi32_si128((int16)x);
    t = _mm_and_si128(_mm_shuffle_epi32(_mm_unpacklo_epi16(t, t), 0), _mm_set1_epi16(0x7FFF));

    c0 = _mm_cmpgt_epi16(t0, t);
    c1 = _mm_cmpgt_epi16(t1, t);

    //_BitScanForward(&bitindex, _mm_movemask_epi8(_mm_packs_epi16(c0, c1)) | 0x10000);
    bitindex = __builtin_ctz(_mm_movemask_epi8(_mm_packs_epi16(c0, c1)) | 0x10000);
    start = model->prob[bitindex - 1];
    end = model->prob[bitindex];

    c0 = _mm_and_si128(_mm_set1_epi16(0x7FD9), c0);
    c1 = _mm_and_si128(_mm_set1_epi16(0x7FD9), c1);

    c0 = _mm_add_epi16(c0, _mm_set_epi16(56, 48, 40, 32, 24, 16, 8, 0));
    c1 = _mm_add_epi16(c1, _mm_set_epi16(120, 112, 104, 96, 88, 80, 72, 64));

    t0 = _mm_add_epi16(_mm_srai_epi16(_mm_sub_epi16(c0, t0), 7), t0);
    t1 = _mm_add_epi16(_mm_srai_epi16(_mm_sub_epi16(c1, t1), 7), t1);

    _mm_storeu_si128((__m128i *)&model->prob[0], t0);
    _mm_storeu_si128((__m128i *)&model->prob[8], t1);

    tab->bits_a = (end - start) * (x >> 15) + (x & 0x7FFF) - start;
    LznaRenormalize(tab);
    return (int)bitindex - 1;
}



// Read a 3-bit value using an adaptive RANS model
static uint32_t __forceinline LznaRead3bit(LznaBitReader *tab, Lzna3bitModel *model)
{
    __m128i t;
    __m128i t0;
    __m128i c0;
    unsigned long bitindex;
    unsigned int start;
    unsigned int end;
    uint64_t x = tab->bits_a;

    t0 = _mm_loadu_si128((const __m128i *)&model->prob[0]);
    t = _mm_cvtsi32_si128(x & 0x7FFF);
    t = _mm_shuffle_epi32(_mm_unpacklo_epi16(t, t), 0);
    c0 = _mm_cmpgt_epi16(t0, t);

    //_BitScanForward(&bitindex, _mm_movemask_epi8(c0) | 0x10000);
    bitindex = __builtin_ctz(_mm_movemask_epi8(c0) | 0x10000);
    bitindex >>= 1;
    start = model->prob[bitindex - 1];
    end = model->prob[bitindex];

    c0 = _mm_and_si128(_mm_set1_epi16(0x7FE5), c0);
    c0 = _mm_add_epi16(c0, _mm_set_epi16(56, 48, 40, 32, 24, 16, 8, 0));
    t0 = _mm_add_epi16(_mm_srai_epi16(_mm_sub_epi16(c0, t0), 7), t0);
    _mm_storeu_si128((__m128i *)&model->prob[0], t0);

    tab->bits_a = (end - start) * (x >> 15) + (x & 0x7FFF) - start;
    LznaRenormalize(tab);
    return bitindex - 1;
}



// Read a 1-bit value using an adaptive RANS model
static uint32_t __forceinline LznaRead1Bit(LznaBitReader *tab, LznaBitModel *model, int nbits, int shift)
{
    uint64_t q;
    int magn = 1 << nbits;
    q = *model * (tab->bits_a >> nbits);

    if ((tab->bits_a & (magn - 1)) >= *model)
    {
        tab->bits_a -= q + *model;
        *model = *model - (*model >> shift);
        LznaRenormalize(tab);
        return 1;
    }
    else
    {
        tab->bits_a = (tab->bits_a & (magn - 1)) + q;
        *model = *model + ((magn - *model) >> shift);
        LznaRenormalize(tab);
        return 0;
    }
}



// Read a far distance using the far distance model
static uint32_t __forceinline LznaReadFarDistance(LznaBitReader *tab, LznaState *lut)
{
    uint32_t n = LznaReadNibble(tab, &lut->far_distance.first_lo);
    uint32_t hi;

    if (n >= 15)
    {
        n = 15 + LznaReadNibble(tab, &lut->far_distance.first_hi);
    }
    hi = 0;
    if (n != 0)
    {
        hi = LznaRead1Bit(tab, &lut->far_distance.second[n - 1], 14, 6) + 2;
        if (n != 1)
        {
            hi = (hi << 1) + LznaRead1Bit(tab, &lut->far_distance.third[hi - 2][n - 1], 14, 6);
            if (n != 2)
            {
                hi = (hi << (n - 2)) + LznaReadNBits(tab, n - 2);
            }
        }
        hi -= 1;
    }
    LznaLowBitsDistanceModel *lutd = &lut->low_bits_of_distance[hi == 0];
    uint32_t low_bit = LznaRead1Bit(tab, &lutd->v, 14, 6);
    uint32_t low_nibble = LznaReadNibble(tab, &lutd->d[low_bit]);
    return low_bit + (2 * low_nibble) + (32 * hi) + 1;
}



// Read a near distance using a near distance model
static uint32_t __forceinline LznaReadNearDistance(LznaBitReader *tab, LznaState *lut, LznaNearDistModel *model)
{
    uint32_t nb = LznaReadNibble(tab, &model->first);
    uint32_t hi = 0;

    if (nb != 0)
    {
        hi = LznaRead1Bit(tab, &model->second[nb - 1], 14, 6) + 2;
        if (nb != 1)
        {
            hi = (hi << 1) + LznaRead1Bit(tab, &model->third[hi - 2][nb - 1], 14, 6);
            if (nb != 2)
            {
                hi = (hi << (nb - 2)) + LznaReadNBits(tab, nb - 2);
            }
        }
        hi -= 1;
    }
    LznaLowBitsDistanceModel *lutd = &lut->low_bits_of_distance[hi == 0];
    uint32_t low_bit = LznaRead1Bit(tab, &lutd->v, 14, 6);
    uint32_t low_nibble = LznaReadNibble(tab, &lutd->d[low_bit]);
    return low_bit + (2 * low_nibble) + (32 * hi) + 1;
}



// Read a length using the length model.
static uint32_t __forceinline LznaReadLength(LznaBitReader *tab, LznaLongLengthModel *model, int64_t dst_offs)
{
    uint32_t length = LznaReadNibble(tab, &model->first[dst_offs & 3]);

    if (length >= 12)
    {
        uint32_t b = LznaReadNibble(tab, &model->second);
        if (b >= 15)
        {
            b = 15 + LznaReadNibble(tab, &model->third);
        }
        uint32_t n = 0;
        uint32_t base = 0;
        if (b)
        {
            n = (b - 1) >> 1;
            base = ((((b - 1) & 1) + 2) << n) - 1;
        }
        length += (LznaReadNBits(tab, n) + base) * 4;
    }
    return length;
}



static const uint8_t next_state_lit[12] = {
    0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5
};



static void LznaCopyLongDist(byte *dst, size_t dist, size_t length)
{
    const byte *src = dst - dist;
    ((uint64_t*)dst)[0] = ((uint64_t*)src)[0];
    ((uint64_t*)dst)[1] = ((uint64_t*)src)[1];

    if (length > 16)
    {
        do {
            ((uint64_t*)dst)[2] = ((uint64_t*)src)[2];
            dst += 8;
            src += 8;
            length -= 8;
        } while (length > 16);
    }
}



static void LznaCopyShortDist(byte *dst, size_t dist, size_t length)
{
    const byte *src = dst - dist;

    if (dist >= 4)
    {
        ((uint32_t*)dst)[0] = ((uint32_t*)src)[0];
        ((uint32_t*)dst)[1] = ((uint32_t*)src)[1];
        ((uint32_t*)dst)[2] = ((uint32_t*)src)[2];
        if (length > 12)
        {
            ((uint32_t*)dst)[3] = ((uint32_t*)src)[3];
            if (length > 16)
            {
                do {
                    ((uint32_t*)dst)[4] = ((uint32_t*)src)[4];
                    length -= 4;
                    dst += 4;
                    src += 4;
                } while (length > 16);
            }
        }
    }
    else if (dist == 1)
    {
        memset(dst, *src, length);
    }
    else
    {
        ((byte*)dst)[0] = ((byte*)src)[0];
        ((byte*)dst)[1] = ((byte*)src)[1];
        ((byte*)dst)[2] = ((byte*)src)[2];
        ((byte*)dst)[3] = ((byte*)src)[3];
        ((byte*)dst)[4] = ((byte*)src)[4];
        ((byte*)dst)[5] = ((byte*)src)[5];
        ((byte*)dst)[6] = ((byte*)src)[6];
        ((byte*)dst)[7] = ((byte*)src)[7];
        ((byte*)dst)[8] = ((byte*)src)[8];
        while (length > 9)
        {
            ((byte*)dst)[9] = ((byte*)src)[9];
            dst += 1;
            src += 1;
            length -= 1;
        }
    }
}



static void LznaCopy4to12(byte *dst, size_t dist, size_t length)
{
    const byte *src = dst - dist;

    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    if (length > 4)
    {
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
        if (length > 8)
        {
            dst[8] = src[8];
            dst[9] = src[9];
            dst[10] = src[10];
            dst[11] = src[11];
        }
    }
}



static void LznaPreprocessMatchHistory(LznaState *lut)
{
    if (lut->match_history[4] >= 0xc000)
    {
        size_t i = 0;
        while (lut->match_history[4 + i] >= 0xC000)
        {
            ++i;
            if (i >= 4)
            {
                lut->match_history[7] = lut->match_history[6];
                lut->match_history[6] = lut->match_history[5];
                lut->match_history[5] = lut->match_history[4];
                lut->match_history[4] = 4;
                return;
            }
        }
        uint32_t t = lut->match_history[i + 4];
        lut->match_history[i + 4] = lut->match_history[i + 3];
        lut->match_history[i + 3] = lut->match_history[i + 2];
        lut->match_history[i + 2] = lut->match_history[i + 1];
        lut->match_history[4] = t;
    }
}



int LZNA_DecodeQuantum(byte *dst, byte *dst_end, byte *dst_start,
                       const byte *src_in, const byte *src_end,
                       LznaState *lut)
{
    LznaBitReader tab;
    uint32_t x;
    uint32_t dst_offs = dst - dst_start;
    uint32_t match_val;
    uint32_t state;
    uint32_t length;
    uint32_t dist;

    LznaPreprocessMatchHistory(lut);
    LznaBitReader_Init(&tab, src_in);
    dist = lut->match_history[4];

    state = 5;
    dst_end -= 8;

    if (dst_offs == 0)
    {
        if (LznaReadBit(&tab))
        {
            x = 0;
        }
        else
        {
            LznaLiteralModel *model = &lut->literal[0];
            x = LznaReadNibble(&tab, &model->upper[0]);
            x = (x << 4) + LznaReadNibble(&tab, (x != 0) ? &model->nomatch[x] : &model->lower[0]);
        }
        *dst++ = x;
        dst_offs += 1;
    }
    while (dst < dst_end)
    {
        match_val = *(dst - dist);

        if (LznaRead1Bit(&tab, &lut->is_literal[(dst_offs & 7) + 8 * state], 13, 5))
        {
            x = LznaReadNibble(&tab, &lut->type[(dst_offs & 7) + 8 * state]);
            if (x == 0)
            {
                // Copy 1 byte from most recent distance
                *dst++ = match_val;
                dst_offs += 1;
                state = (state >= 7) ? 11 : 9;
            }
            else if (x < 4)
            {
                if (x == 1)
                {
                    // Copy count 3-4
                    length = 3 + LznaRead1Bit(&tab, &lut->short_length[state][dst_offs & 3], 14, 4);
                    dist = LznaReadNearDistance(&tab, lut, &lut->near_dist[length - 3]);
                    dst[0] = (dst - dist)[0];
                    dst[1] = (dst - dist)[1];
                    dst[2] = (dst - dist)[2];
                    dst[3] = (dst - dist)[3];
                }
                else if (x == 2)
                {
                    // Copy count 5-12
                    length = 5 + LznaRead3bit(&tab, &lut->medium_length);
                    dist = LznaReadFarDistance(&tab, lut);
                    if (dist >= 8) {
                        ((uint64_t*)dst)[0] = ((uint64_t*)(dst - dist))[0];
                        ((uint64_t*)dst)[1] = ((uint64_t*)(dst - dist))[1];
                    }
                    else
                    {
                        LznaCopy4to12(dst, dist, length);
                    }
                }
                else
                {
                    // Copy count 13-
                    length = LznaReadLength(&tab, &lut->long_length, dst_offs) + 13;
                    dist = LznaReadFarDistance(&tab, lut);
                    if (dist >= 8)
                    {
                        LznaCopyLongDist(dst, dist, length);
                    }
                    else
                    {
                        LznaCopyShortDist(dst, dist, length);
                    }
                }
                state = (state >= 7) ? 10 : 7;
                lut->match_history[7] = lut->match_history[6];
                lut->match_history[6] = lut->match_history[5];
                lut->match_history[5] = lut->match_history[4];
                lut->match_history[4] = dist;
                dst += length;
                dst_offs += length;
            }
            else if (x >= 12)
            {
                // Copy 2 bytes from a recent distance
                size_t idx = x - 12;
                dist = lut->match_history[4 + idx];
                lut->match_history[4 + idx] = lut->match_history[3 + idx];
                lut->match_history[3 + idx] = lut->match_history[2 + idx];
                lut->match_history[2 + idx] = lut->match_history[1 + idx];
                lut->match_history[4] = dist;
                dst[0] = *(dst - dist + 0);
                dst[1] = *(dst - dist + 1);
                state = (state >= 7) ? 11 : 8;
                dst_offs += 2;
                dst += 2;
            }
            else
            {
                size_t idx = (x - 4) >> 1;
                dist = lut->match_history[4 + idx];
                lut->match_history[4 + idx] = lut->match_history[3 + idx];
                lut->match_history[3 + idx] = lut->match_history[2 + idx];
                lut->match_history[2 + idx] = lut->match_history[1 + idx];
                lut->match_history[4] = dist;
                if (x & 1)
                {
                    // Copy 11- bytes from recent distance
                    length = 11 + LznaReadLength(&tab, &lut->long_length_recent, dst_offs);
                    if (dist >= 8)
                    {
                        LznaCopyLongDist(dst, dist, length);
                    }
                    else
                    {
                        LznaCopyShortDist(dst, dist, length);
                    }
                }
                else
                {
                    // Copy 3-10 bytes from recent distance
                    length = 3 + LznaRead3bit(&tab, &lut->short_length_recent[idx].a[dst_offs & 3]);
                    if (dist >= 8)
                    {
                        ((uint64_t*)dst)[0] = ((uint64_t*)(dst - dist))[0];
                        ((uint64_t*)dst)[1] = ((uint64_t*)(dst - dist))[1];
                    }
                    else
                    {
                        LznaCopy4to12(dst, dist, length);
                    }
                }
                state = (state >= 7) ? 11 : 8;
                dst_offs += length;
                dst += length;
            }
        }
        else
        {
            // Output a literal
            LznaLiteralModel *model = &lut->literal[dst_offs & 3];
            x = LznaReadNibble(&tab, &model->upper[match_val >> 4]);
            x = (x << 4) + LznaReadNibble(&tab, ((match_val >> 4) != x) ? &model->nomatch[x] : &model->lower[match_val & 0xF]);
            *dst++ = x;
            dst_offs += 1;
            state = next_state_lit[state];
        }
    }

    if (dst != dst_end)
    {
        return -1;
    }

    *(uint64_t*)dst = (uint32_t)tab.bits_a | (tab.bits_b << 32);

    return (byte*)tab.src - src_in;
}



// LZNA_ParseWholeMatchInfo()
const byte *LZNA_ParseWholeMatchInfo(const byte *p, uint32_t *dist)
{
    uint32_t v = bswap_16(*(uint16_t*)p);

    if (v < 0x8000)
    {
        uint32_t x = 0;
        uint32_t b = 0;
        uint32_t pos = 0;

        for (;;)
        {
            b = p[2];
            p += 1;
            if (b & 0x80)
            {
                break;
            }
            x += (b + 0x80) << pos;
            pos += 7;
        }
        x += (b - 128) << pos;
        *dist = 0x8000 + v + (x << 15) + 1;
        return p + 2;
    }
    else
    {
        *dist = v - 0x8000 + 1;
        return p + 2;
    }
}



// LZNA_ParseQuantumHeader()
const byte *LZNA_ParseQuantumHeader(KrakenQuantumHeader *hdr, const byte *p, bool use_checksum, int raw_len)
{
    uint32_t v = (p[0] << 8) | p[1];
    uint32_t size = v & 0x3FFF;

    if (size != 0x3fff)
    {
        hdr->compressed_size = size + 1;
        hdr->flag1 = (v >> 14) & 1;
        hdr->flag2 = (v >> 15) & 1;
        if (use_checksum)
        {
            hdr->checksum = (p[2] << 16) | (p[3] << 8) | p[4];
            return p + 5;
        }
        else
        {
            return p + 2;
        }
    }

    v >>= 14;

    if (v == 0)
    {
        p = LZNA_ParseWholeMatchInfo(p + 2, &hdr->whole_match_distance);
        hdr->compressed_size = 0;
        return p;
    }

    if (v == 1)
    {
        // memset
        hdr->checksum = p[2];
        hdr->compressed_size = 0;
        hdr->whole_match_distance = 0;
        return p + 3;
    }

    if (v == 2)
    {
        // uncompressed
        hdr->compressed_size = raw_len;
        return p + 2;
    }

    return NULL;
}

