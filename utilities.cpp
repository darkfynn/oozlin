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

#include "utilities.h"



// CountLeadingZeros()
int CountLeadingZeros(uint32_t bits)
{
    unsigned long x;

    //_BitScanReverse(&x, bits);
    x = 32 - __builtin_clz(bits) - 1;
    return 31 - x;
}



// _rotl()
//
// A safe, efficient and portable 32bit _rotl() (originally: rotl32c()) C/C++ implementation for GCC by John Regehr,
// Professor of Computer Science, University of Utah : // https://blog.regehr.org/archives/1063
uint32_t _rotl(uint32_t x, uint32_t n)
{
    assert (n < 32);
    return (x << n) | (x >> (-n & 31));
}



// MallocAligned()
//
// Allocate memory with a specific alignment
void *MallocAligned(size_t size, size_t alignment)
{
    void *x = malloc(size + (alignment - 1) + sizeof(void*)), *x_org = x;
    if (x)
    {
        x = (void*)(((intptr_t)x + alignment - 1 + sizeof(void*)) & ~(alignment - 1));
        ((void**)x)[-1] = x_org;
    }
    return x;
}



// FreeAligned()
// Free memory allocated through |MallocAligned|
void FreeAligned(void *p)
{
    free(((void**)p)[-1]);
}



// BSR()
uint32_t BSR(uint32_t x)
{
    unsigned long index;

    //_BitScanReverse(&index, x);
    index =  32 - __builtin_clz(x) - 1;
    return index;
}



// BSF()
uint32_t BSF(uint32_t x)
{
    unsigned long index;

    //_BitScanForward(&index, x);
    index = __builtin_ctz(x);
    return index;
}



// error()
void error(const char *s, const char *curfile)
{
    if (curfile)
    {
        fprintf(stderr, "%s: ", curfile);
    }
    fprintf(stderr, "%s\n", s);
    exit(1);
}


// load_file()
byte *load_file(const char *filename, int *size)
{
    FILE *f = fopen(filename, "rb");
    if (!f) 
    {
        error("file open error", filename);
    }

    fseek(f, 0, SEEK_END);
    int packed_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    byte *input = new byte[packed_size];
    if (!input)
    {
        error("memory error", filename);
    }

    if (fread(input, 1, packed_size, f) != packed_size)
    {
        error("error reading", filename);
    }

    fclose(f);
    *size = packed_size;
    return input;
}



// FillByteOverflow16()
//
// May overflow 16 bytes past the end
void FillByteOverflow16(uint8_t *dst, uint8_t v, size_t n)
{
    memset(dst, v, n);
}




