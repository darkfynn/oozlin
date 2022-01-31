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

// Mermaid/Selkie decompression also happens in two phases, just like in Kraken,
// but the match copier works differently.
// Both Mermaid and Selkie use the same on-disk format, only the compressor
// differs.
typedef struct MermaidLzTable {
    // Flag stream. Format of flags:
    // Read flagbyte from |cmd_stream|
    // If flagbyte >= 24:
    //   flagbyte & 0x80 == 0 : Read from |off16_stream| into |recent_offs|.
    //                   != 0 : Don't read offset.
    //   flagbyte & 7 = Number of literals to copy first from |lit_stream|.
    //   (flagbyte >> 3) & 0xF = Number of bytes to copy from |recent_offs|.
    //
    //  If flagbyte == 0 :
    //    Read byte L from |length_stream|
    //    If L > 251: L += 4 * Read word from |length_stream|
    //    L += 64
    //    Copy L bytes from |lit_stream|.
    //
    //  If flagbyte == 1 :
    //    Read byte L from |length_stream|
    //    If L > 251: L += 4 * Read word from |length_stream|
    //    L += 91
    //    Copy L bytes from match pointed by next offset from |off16_stream|
    //
    //  If flagbyte == 2 :
    //    Read byte L from |length_stream|
    //    If L > 251: L += 4 * Read word from |length_stream|
    //    L += 29
    //    Copy L bytes from match pointed by next offset from |off32_stream|,
    //    relative to start of block.
    //    Then prefetch |off32_stream[3]|
    //
    //  If flagbyte > 2:
    //    L = flagbyte + 5
    //    Copy L bytes from match pointed by next offset from |off32_stream|,
    //    relative to start of block.
    //    Then prefetch |off32_stream[3]|
    const byte *cmd_stream, *cmd_stream_end;

    // Length stream
    const uint8_t *length_stream;

    // Literal stream
    const uint8_t *lit_stream, *lit_stream_end;

    // Near offsets
    const uint16_t *off16_stream, *off16_stream_end;

    // Far offsets for current chunk
    uint32_t *off32_stream;
    uint32_t *off32_stream_end;

    // Holds the offsets for the two chunks
    uint32_t *off32_stream_1;
    uint32_t *off32_stream_2;
    uint32_t off32_size_1;
    uint32_t off32_size_2;

    // Flag offsets for next 64k chunk.
    uint32_t cmd_stream_2_offs;
    uint32_t cmd_stream_2_offs_end;
} MermaidLzTable;



// Prototypes
int Mermaid_DecodeFarOffsets(const byte *src, const byte *src_end, uint32_t *output, size_t output_size, int64_t offset);
void Mermaid_CombineOffs16(uint16_t *dst, size_t size, const uint8_t *lo, const uint8_t *hi);
bool Mermaid_ReadLzTable(int mode, const byte *src, const byte *src_end, byte *dst,
                         int dst_size, int64_t offset, byte *scratch, byte *scratch_end,
                         MermaidLzTable *lz);
const byte *Mermaid_Mode0(byte *dst, size_t dst_size, byte *dst_ptr_end,
                          byte *dst_start, const byte *src_end, MermaidLzTable *lz,
                          int32_t *saved_dist, size_t startoff);
const byte *Mermaid_Mode1(byte *dst, size_t dst_size, byte *dst_ptr_end, byte *dst_start,
                         const byte *src_end, MermaidLzTable *lz, int32_t *saved_dist,
                         size_t startoff);
bool Mermaid_ProcessLzRuns(int mode, const byte *src, const byte *src_end,
                           byte *dst, size_t dst_size, uint64_t offset,
                           byte *dst_end, MermaidLzTable *lz);
int Mermaid_DecodeQuantum(byte *dst, byte *dst_end, byte *dst_start,
                          const byte *src, const byte *src_end, byte *temp,
                          byte *temp_end);


