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


#include "kraken_bits.h"



// BitReader_Refill
//
// Read more bytes to make sure we always have at least 24 bits in |bits|.
void BitReader_Refill(BitReader *bits)
{
    assert(bits->bitpos <= 24);
    while (bits->bitpos > 0)
    {
        bits->bits |= (bits->p < bits->p_end ? *bits->p : 0) << bits->bitpos;
        bits->bitpos -= 8;
        bits->p++;
    }
}



// BitReader_RefillBackwards()
//
// Read more bytes to make sure we always have at least 24 bits in |bits|,
// used when reading backwards.
void BitReader_RefillBackwards(BitReader *bits)
{
    assert(bits->bitpos <= 24);
    while (bits->bitpos > 0)
    {
        bits->p--;
        bits->bits |= (bits->p >= bits->p_end ? *bits->p : 0) << bits->bitpos;
        bits->bitpos -= 8;
    }
}



// BitReader_ReadBit()
//
// Refill bits then read a single bit.
int BitReader_ReadBit(BitReader *bits)
{
    int r;

    BitReader_Refill(bits);
    r = bits->bits >> 31;
    bits->bits <<= 1;
    bits->bitpos += 1;
    return r;
}



// BitReader_ReadBitNoRefill()
int BitReader_ReadBitNoRefill(BitReader *bits)
{
    int r;

    r = bits->bits >> 31;
    bits->bits <<= 1;
    bits->bitpos += 1;
    return r;
}



// BitReader_ReadBitsNoRefill()
//
// Read |n| bits without refilling.
int BitReader_ReadBitsNoRefill(BitReader *bits, int n)
{
    int r;
   
    r = (bits->bits >> (32 - n));
    bits->bits <<= n;
    bits->bitpos += n;
    return r;
}



// BitReader_ReadBitsNoRefillZero()
//
// Read |n| bits without refilling, n may be zero.
int BitReader_ReadBitsNoRefillZero(BitReader *bits, int n)
{
    int r;
   
    r = (bits->bits >> 1 >> (31 - n));
    bits->bits <<= n;
    bits->bitpos += n;
    return r;
}



// BitReader_ReadMoreThan24Bits()
uint32_t BitReader_ReadMoreThan24Bits(BitReader *bits, int n)
{
    uint32_t rv;

    if (n <= 24)
    {
        rv = BitReader_ReadBitsNoRefillZero(bits, n);
    }
    else
    {
        rv = BitReader_ReadBitsNoRefill(bits, 24) << (n - 24);
        BitReader_Refill(bits);
        rv += BitReader_ReadBitsNoRefill(bits, n - 24);
    }
    BitReader_Refill(bits);
    return rv;
}



// BitReader_ReadMoreThan24BitsB()
uint32_t BitReader_ReadMoreThan24BitsB(BitReader *bits, int n)
{
    uint32_t rv;

    if (n <= 24)
    {
        rv = BitReader_ReadBitsNoRefillZero(bits, n);
    }
    else
    {
        rv = BitReader_ReadBitsNoRefill(bits, 24) << (n - 24);
        BitReader_RefillBackwards(bits);
        rv += BitReader_ReadBitsNoRefill(bits, n - 24);
    }
    BitReader_RefillBackwards(bits);
    return rv;
}



// BitReader_ReadGamma()
//
// Reads a gamma value.
// Assumes bitreader is already filled with at least 23 bits
int BitReader_ReadGamma(BitReader *bits)
{
    unsigned long bitresult;
    int n;
    int r;

    if (bits->bits != 0)
    {
        //_BitScanReverse(&bitresult, bits->bits);
        bitresult = 32 - __builtin_clz(bits->bits) - 1;
        n = 31 - bitresult;
    }
    else
    {
        n = 32;
    }

    n = 2 * n + 2;
    assert(n < 24);
    bits->bitpos += n;
    r = bits->bits >> (32 - n);
    bits->bits <<= n;
    return r - 2;
}



// BitReader_ReadGammaX()
//
// Reads a gamma value with |forced| number of forced bits.
int BitReader_ReadGammaX(BitReader *bits, int forced)
{
    unsigned long bitresult;
    int r;

    if (bits->bits != 0)
    {
        //_BitScanReverse(&bitresult, bits->bits);
        bitresult = 32 - __builtin_clz(bits->bits) - 1;
        int lz = 31 - bitresult;
        assert(lz < 24);
        r = (bits->bits >> (31 - lz - forced)) + ((lz - 1) << forced);
        bits->bits <<= lz + forced + 1;
        bits->bitpos += lz + forced + 1;
        return r;
    }
    return 0;
}



// BitReader_ReadDistance()
//
// Reads a offset code parametrized by |v|.
uint32_t BitReader_ReadDistance(BitReader *bits, uint32_t v)
{
    uint32_t w;
    uint32_t m;
    uint32_t n;
    uint32_t rv;

    if (v < 0xF0)
    {
        n = (v >> 4) + 4;
        w = _rotl(bits->bits | 1, n);
        bits->bitpos += n;
        m = (2 << n) - 1;
        bits->bits = w & ~m;
        rv = ((w & m) << 4) + (v & 0xF) - 248;
    }
    else
    {
        n = v - 0xF0 + 4;
        w = _rotl(bits->bits | 1, n);
        bits->bitpos += n;
        m = (2 << n) - 1;
        bits->bits = w & ~m;
        rv = 8322816 + ((w & m) << 12);
        BitReader_Refill(bits);
        rv += (bits->bits >> 20);
        bits->bitpos += 12;
        bits->bits <<= 12;
    }

    BitReader_Refill(bits);
    return rv;
}



// BitReader_ReadDistanceB()
//
// Reads a offset code parametrized by |v|, backwards.
uint32_t BitReader_ReadDistanceB(BitReader *bits, uint32_t v)
{
    uint32_t w;
    uint32_t m;
    uint32_t n;
    uint32_t rv;

    if (v < 0xF0)
    {
        n = (v >> 4) + 4;
        w = _rotl(bits->bits | 1, n);
        bits->bitpos += n;
        m = (2 << n) - 1;
        bits->bits = w & ~m;
        rv = ((w & m) << 4) + (v & 0xF) - 248;
    }
    else
    {
        n = v - 0xF0 + 4;
        w = _rotl(bits->bits | 1, n);
        bits->bitpos += n;
        m = (2 << n) - 1;
        bits->bits = w & ~m;
        rv = 8322816 + ((w & m) << 12);
        BitReader_RefillBackwards(bits);
        rv += (bits->bits >> (32 - 12));
        bits->bitpos += 12;
        bits->bits <<= 12;
    }

    BitReader_RefillBackwards(bits);
    return rv;
}



// BitReader_ReadLength()
//
// Reads a length code.
bool BitReader_ReadLength(BitReader *bits, uint32_t *v)
{
    unsigned long bitresult;
    int n;
    uint32_t rv;

    //_BitScanReverse(&bitresult, bits->bits);
    bitresult = 32 - __builtin_clz(bits->bits) - 1;
    n = 31 - bitresult;
    if (n > 12)
    {
        return false;
    }
    bits->bitpos += n;
    bits->bits <<= n;
    BitReader_Refill(bits);
    n += 7;
    bits->bitpos += n;
    rv = (bits->bits >> (32 - n)) - 64;
    bits->bits <<= n;
    *v = rv;
    BitReader_Refill(bits);
    return true;
}



// BitReader_ReadLengthB()
//
// Reads a length code, backwards.
bool BitReader_ReadLengthB(BitReader *bits, uint32_t *v)
{
    unsigned long bitresult;
    int n;
    uint32_t rv;

    //_BitScanReverse(&bitresult, bits->bits);
    bitresult = 32 - __builtin_clz(bits->bits) - 1;
    n = 31 - bitresult;
    if (n > 12)
    {
        return false;
    }
    bits->bitpos += n;
    bits->bits <<= n;
    BitReader_RefillBackwards(bits);
    n += 7;
    bits->bitpos += n;
    rv = (bits->bits >> (32 - n)) - 64;
    bits->bits <<= n;
    *v = rv;
    BitReader_RefillBackwards(bits);
    return true;
}



// BitReader_ReadFluff()
int BitReader_ReadFluff(BitReader *bits, int num_symbols)
{
    unsigned long y;

    if (num_symbols == 256)
    {
        return 0;
    }

    int x = 257 - num_symbols;
    if (x > num_symbols)
    {
        x = num_symbols;
    }

    x *= 2;

    //_BitScanReverse(&y, x - 1);
    y = 32 - __builtin_clz(x - 1) - 1;
    y += 1;

    uint32_t v = bits->bits >> (32 - y);
    uint32_t z = (1 << y) - x;

    if ((v >> 1) >= z)
    {
        bits->bits <<= y;
        bits->bitpos += y;
        return v - z;
    }
    else
    {
        bits->bits <<= (y - 1);
        bits->bitpos += (y - 1);
        return (v >> 1);
    }
}



// DecodeGolombRiceLengths()
bool DecodeGolombRiceLengths(uint8_t *dst, size_t size, BitReader2 *br)
{
    const uint8_t *p = br->p,
    const uint8_t *p_end = br->p_end;
    uint8_t *dst_end = dst + size;
    if (p >= p_end)
    {
        return false;
    }

    int count = -(int)br->bitpos;
    uint32_t v = *p++ & (255 >> br->bitpos);
    for (;;)
    {
        if (v == 0)
        {
            count += 8;
        }
        else
        {
            uint32_t x = kRiceCodeBits2Value[v];
            *(uint32_t*)&dst[0] = count + (x & 0x0f0f0f0f);
            *(uint32_t*)&dst[4] = (x >> 4) & 0x0f0f0f0f;
            dst += kRiceCodeBits2Len[v];
            if (dst >= dst_end)
            {
                break;
            }
            count = x >> 28;
        }
        if (p >= p_end)
        {
            return false;
        }
        v = *p++;
    }
    // went too far, step back
    if (dst > dst_end)
    {
        int n = dst - dst_end;
        do { 
            v &= (v - 1);
        } while (--n);
    }
    // step back if byte not finished
    int bitpos = 0;
    if (!(v & 1))
    {
        p--;
        unsigned long q;
        //_BitScanForward(&q, v);
        q = __builtin_ctz(v);
        bitpos = 8 - q;
    }
    br->p = p;
    br->bitpos = bitpos;
    return true;
}



// DecodeGolombRiceBits()
bool DecodeGolombRiceBits(uint8_t *dst, uint size, uint bitcount, BitReader2 *br)
{
    if (bitcount == 0)
    {
        return true;
    }
    uint8_t *dst_end = dst + size;
    const uint8_t *p = br->p;
    int bitpos = br->bitpos;

    uint bits_required = bitpos + bitcount * size;
    uint bytes_required = (bits_required + 7) >> 3;
    if (bytes_required > br->p_end - p)
    {
        return false;
    }

    br->p = p + (bits_required >> 3);
    br->bitpos = bits_required & 7;

    // todo. handle r/w outside of range
    uint64_t bak = *(uint64_t*)dst_end;

    if (bitcount < 2)
    {
        assert(bitcount == 1);
        do {
            // Read the next byte
            uint64_t bits = (uint8_t)(bswap_32(*(uint32_t*)p) >> (24 - bitpos));
            p += 1;
            // Expand each bit into each byte of the uint64_t.
            bits = (bits | (bits << 28)) & 0xF0000000Full;
            bits = (bits | (bits << 14)) & 0x3000300030003ull;
            bits = (bits | (bits <<  7)) & 0x0101010101010101ull;
            *(uint64_t*)dst = *(uint64_t*)dst * 2 + bswap_64(bits);
            dst += 8;
        } while (dst < dst_end);
    }
    else if (bitcount == 2)
    {
        do {
            // Read the next 2 bytes
            uint64_t bits = (uint16_t)(bswap_32(*(uint32_t*)p) >> (16 - bitpos));
            p += 2;
            // Expand each bit into each byte of the uint64_t.
            bits = (bits | (bits << 24)) & 0xFF000000FFull;
            bits = (bits | (bits << 12)) & 0xF000F000F000Full;
            bits = (bits | (bits << 6)) & 0x0303030303030303ull;
            *(uint64_t*)dst = *(uint64_t*)dst * 4 + bswap_64(bits);
            dst += 8;
        } while (dst < dst_end);
    }
    else
    {
        assert(bitcount == 3);
        do {
            // Read the next 3 bytes
            uint64_t bits = (bswap_32(*(uint32_t*)p) >> (8 - bitpos)) & 0xffffff;
            p += 3;
            // Expand each bit into each byte of the uint64_t.
            bits = (bits | (bits << 20)) & 0xFFF00000FFFull;
            bits = (bits | (bits << 10)) & 0x3F003F003F003Full;
            bits = (bits | (bits << 5)) & 0x0707070707070707ull;
            *(uint64_t*)dst = *(uint64_t*)dst * 8 + bswap_64(bits);
            dst += 8;
        } while (dst < dst_end);
    }
    *(uint64_t*)dst_end = bak;
    return true;
}




