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


// State for a 4-bit value RANS model
struct LznaNibbleModel {
    uint16_t prob[17];
};

// State for a 3-bit value RANS model
struct Lzna3bitModel {
    uint16_t prob[9];
};

// State for the literal model
struct LznaLiteralModel {
    LznaNibbleModel upper[16];
    LznaNibbleModel lower[16];
    LznaNibbleModel nomatch[16];
};

// State for a model representing a far distance
struct LznaFarDistModel {
    LznaNibbleModel first_lo;
    LznaNibbleModel first_hi;
    LznaBitModel second[31];
    LznaBitModel third[2][31];
};

// State for a model representing a near distance
struct LznaNearDistModel {
    LznaNibbleModel first;
    LznaBitModel second[16];
    LznaBitModel third[2][16];
};

// State for model representing the low bits of a distance
struct LznaLowBitsDistanceModel {
    LznaNibbleModel d[2];
    LznaBitModel v;
};

// State for model used for the short lengths for recent matches
struct LznaShortLengthRecentModel {
    Lzna3bitModel a[4];
};

// State for model for long lengths
struct LznaLongLengthModel {
    LznaNibbleModel first[4];
    LznaNibbleModel second;
    LznaNibbleModel third;
};

// Complete LZNA state
struct LznaState {
    uint32_t match_history[8];
    LznaLiteralModel literal[4];
    LznaBitModel is_literal[12 * 8];
    LznaNibbleModel type[12 * 8];
    LznaShortLengthRecentModel short_length_recent[4];
    LznaLongLengthModel long_length_recent;
    LznaLowBitsDistanceModel low_bits_of_distance[2];
    LznaBitModel short_length[12][4];
    LznaNearDistModel near_dist[2];
    Lzna3bitModel medium_length;
    LznaLongLengthModel long_length;
    LznaFarDistModel far_distance;
};

struct LznaBitReader {
    uint64_t bits_a;
    uint64_t bits_b;
    const uint32_t *src;
    const uint32_t *src_start;
};



// Prototypes
static void LznaNibbleModel_Init(LznaNibbleModel *d);
static void Lzna3bitModel_Init(Lzna3bitModel *d);
static void LznaNibbleModel_InitN(LznaNibbleModel *d, int n);
static void LznaLiteralModel_InitN(LznaLiteralModel *d, int n);
static void LznaShortLengthRecentModel_InitN(LznaShortLengthRecentModel *d, int n);
static void LznaNearDistModel_Init(LznaNearDistModel *d, int n);
static void LznaLowBitsDistanceModel_Init(LznaLowBitsDistanceModel *d, int n);
static void LznaFarDistModel_Init(LznaFarDistModel *d);
void LZNA_InitLookup(LznaState *lut);
static void LznaBitReader_Init(LznaBitReader *tab, const byte *src);
static void __forceinline LznaRenormalize(LznaBitReader *tab);
static uint32_t __forceinline LznaReadBit(LznaBitReader *tab);
static uint32_t __forceinline LznaReadNBits(LznaBitReader *tab, int bits);
static uint32_t __forceinline LznaReadNibble(LznaBitReader *tab, LznaNibbleModel *model);
static uint32_t __forceinline LznaRead3bit(LznaBitReader *tab, Lzna3bitModel *model);
static uint32_t __forceinline LznaRead1Bit(LznaBitReader *tab, LznaBitModel *model, int nbits, int shift);
static uint32_t __forceinline LznaReadFarDistance(LznaBitReader *tab, LznaState *lut);
static uint32_t __forceinline LznaReadNearDistance(LznaBitReader *tab, LznaState *lut, LznaNearDistModel *model);
static uint32_t __forceinline LznaReadLength(LznaBitReader *tab, LznaLongLengthModel *model, int64_t dst_offs);
static void LznaCopyLongDist(byte *dst, size_t dist, size_t length);
static void LznaCopyShortDist(byte *dst, size_t dist, size_t length);
static void LznaCopy4to12(byte *dst, size_t dist, size_t length);
static void LznaPreprocessMatchHistory(LznaState *lut);
int LZNA_DecodeQuantum(byte *dst, byte *dst_end, byte *dst_start, const byte *src,
                       const byte *src_end, struct LznaState *lut);
const byte *LZNA_ParseWholeMatchInfo(const byte *p, uint32_t *dist);
const byte *LZNA_ParseQuantumHeader(KrakenQuantumHeader *hdr, const byte *p, bool use_checksum, int raw_len);


