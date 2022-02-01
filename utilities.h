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


// The decompressor will write outside of the target buffer.
#define SAFE_SPACE 64

// Global Vars
enum {
    kCompressor_Kraken = 8,
    kCompressor_Mermaid = 9,
    kCompressor_Selkie = 11,
    kCompressor_Hydra = 12,
    kCompressor_Leviathan = 13,
};



// typedef's
// XXX Need to remove this!!
//typedef int OodLZ_CompressFunc(int codec, uint8_t *src_buf, size_t src_len,
//                               uint8_t *dst_buf, int level, void *opts,
//                               size_t offs, size_t unused, void *scratch,
//                               size_t scratch_size);

//typedef int OodLZ_DecompressFunc(uint8_t *src_buf, int src_len, uint8_t *dst,
//                                 size_t dst_size, int fuzz, int crc, int verbose,
//                                 uint8_t *dst_base, size_t e, void *cb, void *cb_ctx,
//                                 void *scratch, size_t scratch_size, int threadPhase);



// Prototypes
int CountLeadingZeros(uint32_t bits);
uint32_t _rotl(uint32_t x, uint32_t n);
void *MallocAligned(size_t size, size_t alignment);
void FreeAligned(void *p);
uint32_t BSR(uint32_t x);
uint32_t BSF(uint32_t x);
void error(const char *s, const char *curfile = NULL);
byte *load_file(const char *filename, int *size);
int ParseCmdLine(int argc, char *argv[]);
bool Verify(const char *filename, uint8_t *output, int outbytes, const char *curfile);
void FillByteOverflow16(uint8_t *dst, uint8_t v, size_t n);
void LoadLib();
