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

#include "huff.h"

// Huff_ReadCodeLengthsOld()
int Huff_ReadCodeLengthsOld(BitReader *bits, uint8_t *syms, uint32_t *code_prefix)
{
    if (BitReader_ReadBitNoRefill(bits))
    {
        int n, sym = 0, codelen, num_symbols = 0;
        int avg_bits_x4 = 32;
        int forced_bits = BitReader_ReadBitsNoRefill(bits, 2);

        uint32 thres_for_valid_gamma_bits = 1 << (31 - (20u >> forced_bits));
        if (BitReader_ReadBit(bits))
        {
            goto SKIP_INITIAL_ZEROS;
        }
        do
        {
            // Run of zeros
            if (!(bits->bits & 0xff000000))
            {
                return -1;
            }
            sym += BitReader_ReadBitsNoRefill(bits, 2 * (CountLeadingZeros(bits->bits) + 1)) - 2 + 1;
            if (sym >= 256)
            {
                break;
            }
SKIP_INITIAL_ZEROS:
            BitReader_Refill(bits);
            // Read out the gamma value for the # of symbols
            if (!(bits->bits & 0xff000000))
            {
                return -1;
            }
            n = BitReader_ReadBitsNoRefill(bits, 2 * (CountLeadingZeros(bits->bits) + 1)) - 2 + 1;
            // Overflow?
            if (sym + n > 256)
            {
                return -1;
            }
            BitReader_Refill(bits);
            num_symbols += n;
            do
            {
                if (bits->bits < thres_for_valid_gamma_bits)
                {
                    return -1; // too big gamma value?
                }

                int lz = CountLeadingZeros(bits->bits);
                int v = BitReader_ReadBitsNoRefill(bits, lz + forced_bits + 1) + ((lz - 1) << forced_bits);
                codelen = (-(int)(v & 1) ^ (v >> 1)) + ((avg_bits_x4 + 2) >> 2);
                if (codelen < 1 || codelen > 11)
                {
                    return -1;
                }
                avg_bits_x4 = codelen + ((3 * avg_bits_x4 + 2) >> 2);
                BitReader_Refill(bits);
                syms[code_prefix[codelen]++] = sym++;
            } while (--n);
        } while (sym != 256);
        return (sym == 256) && (num_symbols >= 2) ? num_symbols : -1;
    }
    else
    {
        // Sparse symbol encoding
        int num_symbols = BitReader_ReadBitsNoRefill(bits, 8);
        if (num_symbols == 0)
        {
            return -1;
        }
        if (num_symbols == 1)
        {
            syms[0] = BitReader_ReadBitsNoRefill(bits, 8);
        }
        else
        {
            int codelen_bits = BitReader_ReadBitsNoRefill(bits, 3);
            if (codelen_bits > 4)
            {
                return -1;
            }
            for (int i = 0; i < num_symbols; i++)
            {
                BitReader_Refill(bits);
                int sym = BitReader_ReadBitsNoRefill(bits, 8);
                int codelen = BitReader_ReadBitsNoRefillZero(bits, codelen_bits) + 1;
                if (codelen > 11)
                {
                    return -1;
                }
                syms[code_prefix[codelen]++] = sym;
            }
        }
        return num_symbols;
    }
}



// Huff_ConvertToRanges()
int Huff_ConvertToRanges(HuffRange *range, int num_symbols, int P, const uint8_t *symlen, BitReader *bits)
{
    int num_ranges = P >> 1, v, sym_idx = 0;

    // Start with space?
    if (P & 1)
    {
        BitReader_Refill(bits);
        v = *symlen++;
        if (v >= 8)
        {
            return -1;
        }
        sym_idx = BitReader_ReadBitsNoRefill(bits, v + 1) + (1 << (v + 1)) - 1;
    }
    int syms_used = 0;

    for (int i = 0; i < num_ranges; i++)
    {
        BitReader_Refill(bits);
        v = symlen[0];
        if (v >= 9)
        {
            return -1;
        }
        int num = BitReader_ReadBitsNoRefillZero(bits, v) + (1 << v);
        v = symlen[1];
        if (v >= 8)
        {
            return -1;
        }
        int space = BitReader_ReadBitsNoRefill(bits, v + 1) + (1 << (v + 1)) - 1;
        range[i].symbol = sym_idx;
        range[i].num = num;
        syms_used += num;
        sym_idx += num + space;
        symlen += 2;
    }

    if (sym_idx >= 256 || syms_used >= num_symbols || sym_idx + num_symbols - syms_used > 256)
    {
        return -1;
    }

    range[num_ranges].symbol = sym_idx;
    range[num_ranges].num = num_symbols - syms_used;

    return num_ranges + 1;
}



// Huff_ReadCodeLengthsNew()
int Huff_ReadCodeLengthsNew(BitReader *bits, uint8_t *syms, uint32_t *code_prefix)
{
    int forced_bits = BitReader_ReadBitsNoRefill(bits, 2);
    int num_symbols = BitReader_ReadBitsNoRefill(bits, 8) + 1;
    int fluff = BitReader_ReadFluff(bits, num_symbols);

    uint8_t code_len[512];
    BitReader2 br2;
    br2.bitpos = (bits->bitpos - 24) & 7;
    br2.p_end = bits->p_end;
    br2.p = bits->p - (unsigned)((24 - bits->bitpos + 7) >> 3);

    if (!DecodeGolombRiceLengths(code_len, num_symbols + fluff, &br2))
    {
        return -1;
    }
    memset(code_len + (num_symbols + fluff), 0, 16);
    if (!DecodeGolombRiceBits(code_len, num_symbols, forced_bits, &br2))
    {
        return -1;
    }

    // Reset the bits decoder.
    bits->bitpos = 24;
    bits->p = br2.p;
    bits->bits = 0;
    BitReader_Refill(bits);
    bits->bits <<= br2.bitpos;
    bits->bitpos += br2.bitpos;

    if (1)
    {
        uint running_sum = 0x1e;
        int maxlen = 11;
        for (int i = 0; i < num_symbols; i++)
        {
            int v = code_len[i];
            v = -(int)(v & 1) ^ (v >> 1);
            code_len[i] = v + (running_sum >> 2) + 1;
            if (code_len[i] < 1 || code_len[i] > 11)
            {
                return -1;
            }
            running_sum += v;
        }
    }
    else
    {
        // Ensure we don't read unknown data that could contaminate
        // max_codeword_len.
        __m128i bak = _mm_loadu_si128((__m128i*)&code_len[num_symbols]);
        _mm_storeu_si128((__m128i*)&code_len[num_symbols], _mm_set1_epi32(0));
        // apply a filter
        __m128i avg = _mm_set1_epi8(0x1e);
        __m128i ones = _mm_set1_epi8(1);
        __m128i max_codeword_len = _mm_set1_epi8(10);
        for (uint i = 0; i < num_symbols; i += 16)
        {
            __m128i v = _mm_loadu_si128((__m128i*)&code_len[i]), t;
            // avg[0..15] = avg[15]
            avg = _mm_unpackhi_epi8(avg, avg);
            avg = _mm_unpackhi_epi8(avg, avg);
            avg = _mm_shuffle_epi32(avg, 255);
            // v = -(int)(v & 1) ^ (v >> 1)
            v = _mm_xor_si128(_mm_sub_epi8(_mm_set1_epi8(0), _mm_and_si128(v, ones)),
                              _mm_and_si128(_mm_srli_epi16(v, 1), _mm_set1_epi8(0x7f)));
            // create all the sums. v[n] = v[0] + ... + v[n]
            t = _mm_add_epi8(_mm_slli_si128(v, 1), v);
            t = _mm_add_epi8(_mm_slli_si128(t, 2), t);
            t = _mm_add_epi8(_mm_slli_si128(t, 4), t);
            t = _mm_add_epi8(_mm_slli_si128(t, 8), t);
            // u[x] = (avg + t[x-1]) >> 2
            __m128i u = _mm_and_si128(_mm_srli_epi16(_mm_add_epi8(_mm_slli_si128(t, 1), avg), 2u), _mm_set1_epi8(0x3f));
            // v += u
            v = _mm_add_epi8(v, u);
            // avg += t
            avg = _mm_add_epi8(avg, t);
            // max_codeword_len = max(max_codeword_len, v)
            max_codeword_len = _mm_max_epu8(max_codeword_len, v);
            // mem[] = v+1
            _mm_storeu_si128((__m128i*)&code_len[i], _mm_add_epi8(v, _mm_set1_epi8(1)));
        }
        _mm_storeu_si128((__m128i*)&code_len[num_symbols], bak);
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(max_codeword_len, _mm_set1_epi8(10))) != 0xffff)
        {
            return -1; // codeword too big?
        }
    }

    HuffRange range[128];
    int ranges = Huff_ConvertToRanges(range, num_symbols, fluff, &code_len[num_symbols], bits);
    if (ranges <= 0)
    {
        return -1;
    }

    uint8 *cp = code_len;
    for (int i = 0; i < ranges; i++)
    {
        int sym = range[i].symbol;
        int n = range[i].num;
        do
        {
            syms[code_prefix[*cp++]++] = sym++;
        } while (--n);
    }

    return num_symbols;
}



// Huff_MakeLut()
bool Huff_MakeLut(const uint32_t *prefix_org, const uint32_t *prefix_cur, NewHuffLut *hufflut, uint8_t *syms)
{
    uint32_t currslot = 0;

    for(uint32_t i = 1; i < 11; i++)
    {
        uint32_t start = prefix_org[i];
        uint32_t count = prefix_cur[i] - start;
        if (count)
        {
            uint32_t stepsize = 1 << (11 - i);
            uint32_t num_to_set = count << (11 - i);
            if (currslot + num_to_set > 2048)
            {
                return false;
            }
            FillByteOverflow16(&hufflut->bits2len[currslot], i, num_to_set);

            uint8_t *p = &hufflut->bits2sym[currslot];
            for (uint32_t j = 0; j != count; j++, p += stepsize)
            {
                FillByteOverflow16(p, syms[start + j], stepsize);
            }
            currslot += num_to_set;
        }
    }
    if (prefix_cur[11] - prefix_org[11] != 0)
    {
        uint32_t num_to_set = prefix_cur[11] - prefix_org[11];
        if (currslot + num_to_set > 2048)
        {
             return false;
        }
        FillByteOverflow16(&hufflut->bits2len[currslot], 11, num_to_set);
        memcpy(&hufflut->bits2sym[currslot], &syms[prefix_org[11]], num_to_set);
        currslot += num_to_set;
    }
    return currslot == 2048;
}

