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

#define finline __forceinline


struct LeviathanLzTable {
    int *offs_stream;
    int offs_stream_size;
    int *len_stream;
    int len_stream_size;
    uint8_t *lit_stream[16];
    int lit_stream_size[16];
    int lit_stream_total;
    uint8_t *multi_cmd_ptr[8];
    uint8_t *multi_cmd_end[8];
    uint8_t *cmd_stream;
    int cmd_stream_size;
};



// Prototypes
bool Leviathan_ReadLzTable(int chunk_type, const byte *src, const byte *src_end,
                           byte *dst, int dst_size, int offset, byte *scratch,
                           byte *scratch_end, LeviathanLzTable *lztable);
template<typename Mode, bool MultiCmd>
bool Leviathan_ProcessLz(LeviathanLzTable *lzt, uint8_t *dst, uint8_t *dst_start,
                         uint8_t *dst_end, uint8_t *window_base);
bool Leviathan_ProcessLzRuns(int chunk_type, byte *dst, int dst_size, int offset, LeviathanLzTable *lzt);
int Leviathan_DecodeQuantum(byte *dst, byte *dst_end, byte *dst_start,
                            const byte *src, const byte *src_end,
                            byte *scratch, byte *scratch_end);



