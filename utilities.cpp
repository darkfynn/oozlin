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

// Global Vars
enum {
    kCompressor_Kraken = 8,
    kCompressor_Mermaid = 9,
    kCompressor_Selkie = 11, 
    kCompressor_Hydra = 12, 
    kCompressor_Leviathan = 13, 
};

bool arg_stdout;
bool arg_force;
bool arg_quiet;
bool arg_dll;
int arg_compressor = kCompressor_Kraken,
int arg_level = 4;
char arg_direction;
char *verifyfolder;




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
void error(const char *s, const char *curfile = NULL)
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



// ParseCmdLine()
int ParseCmdLine(int argc, char *argv[])
{
    int i;
    // parse command line
    for (i = 1; i < argc; i++)
    {
        char *s = argv[i],
        char *c;
        if (*s != '-')
        {
            break;
        }
        if (*++s == '-')
        {
            if (*++s == 0)
            {
                i++;
                break;  // --
            }
            // long opts
            if (!strcmp(s, "stdout"))
            {
                s = (char *)"c";
            }
            else if (!strcmp(s, "decompress"))
            {
                s = (char *)"d";
            }
            else if (!strcmp(s, "compress"))
            {
                s = (char *)"z";
            }
            else if (!strncmp(s, "verify=",7))
            {
                verifyfolder = s + 7;
                continue;
            } else if (!strcmp(s, "verify")) {
                arg_direction = 't';
                continue;
            } else if (!strcmp(s, "dll"))
            {
                arg_dll = true;
                continue;
            } else if (!strcmp(s, "kraken"))
            {
                s = (char *)"mk";
            }
            else if (!strcmp(s, "mermaid"))
            {
                s = (char *)"mm";
            }
            else if (!strcmp(s, "selkie"))
            {
                s = (char *)"ms";
            }
            else if (!strcmp(s, "leviathan"))
            {
                s = (char *)"ml";
            }
            else if (!strcmp(s, "hydra"))
            {
                s = (char *)"mh";
            }
            else if (!strncmp(s, "level=", 6))
            {
                arg_level = atoi(s + 6);
                continue;
            }
            else
            {
                return -1;
            }
        }
        // short opt
        do {
            switch (c = *s++) {
            case 'z':
            case 'd':
            case 'b':
                if (arg_direction)
                {
                    return -1;
                }
                arg_direction = c;
                break;
            case 'c':
                arg_stdout = true;
                break;
            case 'f':
                arg_force = true;
                break;
            case 'q':
                arg_quiet = true;
                break;
            case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                arg_level = c - '0';
                break;
            case 'm':
                c = *s++;
                arg_compressor = (c == 'k') ? kCompressor_Kraken :
                                 (c == 'm') ? kCompressor_Mermaid :
                                 (c == 's') ? kCompressor_Selkie :
                                 (c == 'l') ? kCompressor_Leviathan :
                                 (c == 'h') ? kCompressor_Hydra : -1;
                if (arg_compressor < 0)
                {
                    return -1;
                }
                break;
            default:
                return -1;
            }
        } while (*s);
    }
    return i;
}



// Verify()
bool Verify(const char *filename, uint8_t *output, int outbytes, const char *curfile)
{
    int test_size;
    byte *test = load_file(filename, &test_size);
    if (!test)
    {
        fprintf(stderr, "file open error: %s\n", filename);
        return false;
    }
    if (test_size != outbytes)
    {
        fprintf(stderr, "%s: ERROR: File size difference: %d vs %d\n", filename, outbytes, test_size);
        return false;
    }
    for (int i = 0; i != test_size; i++)
    {
        if (test[i] != output[i])
        {
            fprintf(stderr, "%s: ERROR: File difference at 0x%x. Was %d instead of %d\n", curfile, i, output[i], test[i]);
            return false;
        }
    }
    return true;
}



// FillByteOverflow16()
//
// May overflow 16 bytes past the end
void FillByteOverflow16(uint8_t *dst, uint8_t v, size_t n)
{
    memset(dst, v, n);
}




