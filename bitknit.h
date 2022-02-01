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

struct BitknitLiteral {
  uint16 lookup[512 + 4]; 
  uint16 a[300 + 1]; 
  uint16 freq[300];
  uint32 adapt_interval;
};

struct BitknitDistanceLsb {
  uint16 lookup[64 + 4]; 
  uint16 a[40 + 1]; 
  uint16 freq[40];
  uint32 adapt_interval;
};

struct BitknitDistanceBits {
  uint16 lookup[64 + 4]; 
  uint16 a[21 + 1]; 
  uint16 freq[21];
  uint32 adapt_interval;
};


struct BitknitState {
  uint32 recent_dist[8];
  uint32 last_match_dist;
  uint32 recent_dist_mask;
  uint32 bits, bits2;

  BitknitLiteral literals[4];
  BitknitDistanceLsb distance_lsb[4];
  BitknitDistanceBits distance_bits;
};


// Prototypes
void BitknitLiteral_Init(BitknitLiteral *model);
void BitknitDistanceLsb_Init(BitknitDistanceLsb *model);
void BitknitDistanceBits_Init(BitknitDistanceBits *model);
void BitknitState_Init(BitknitState *bk);
void BitknitLiteral_Adaptive(BitknitLiteral *model, uint32_t sym);
uint32_t BitknitLiteral_Lookup(BitknitLiteral *model, uint32_t *bits);
void BitknitDistanceLsb_Adaptive(BitknitDistanceLsb *model, uint32_t sym);
uint32_t BitknitDistanceLsb_Lookup(BitknitDistanceLsb *model, uint32_t *bits);
void BitknitDistanceBits_Adaptive(BitknitDistanceBits *model, uint32_t sym);
uint32_t BitknitDistanceBits_Lookup(BitknitDistanceBits *model, uint32_t *bits);
static void BitknitCopyLongDist(byte *dst, size_t dist, size_t length);
static void BitknitCopyShortDist(byte *dst, size_t dist, size_t length);
size_t Bitknit_Decode(const byte *src, const byte *src_end, byte *dst,
                      byte *dst_end, byte *dst_start, BitknitState *bk);


