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
#include "kraken_bits.h"


struct HuffRange {
    uint16_t symbol;
    uint16_t num;
};


struct NewHuffLut {
    // Mapping that maps a bit pattern to a code length.
    uint8_t bits2len[2048 + 16];

    // Mapping that maps a bit pattern to a symbol.
    uint8_t bits2sym[2048 + 16];
};


// Prototype
int Huff_ReadCodeLengthsOld(BitReader *bits, uint8_t *syms, uint32_t *code_prefix);
int Huff_ConvertToRanges(HuffRange *range, int num_symbols, int P, const uint8_t *symlen, BitReader *bits);
int Huff_ReadCodeLengthsNew(BitReader *bits, uint8_t *syms, uint32_t *code_prefix);
bool Huff_MakeLut(const uint32_t *prefix_org, const uint32_t *prefix_cur, NewHuffLut *hufflut, uint8_t *syms);

