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


#include "leviathan.h"



// Leviathan_ReadLzTable()
bool Leviathan_ReadLzTable(int chunk_type, const byte *src, const byte *src_end,
                           byte *dst, int dst_size, int offset, byte *scratch,
                           byte *scratch_end, LeviathanLzTable *lztable)
{
    byte *packed_offs_stream;
    byte *packed_len_stream;
    byte *out;
    int decode_count;
    int n;

    if (chunk_type > 5)
    {
        return false;
    }

    if (src_end - src < 13)
    {
        return false;
    }

    if (offset == 0)
    {
        COPY_64(dst, src);
        dst += 8;
        src += 8;
    }

    int offs_scaling = 0;
    uint8 *packed_offs_stream_extra = NULL;


    int offs_stream_limit = dst_size / 3;

    if (!(src[0] & 0x80))
    {
        // Decode packed offset stream, it's bounded by the command length.
        packed_offs_stream = scratch;
        n = Kraken_DecodeBytes(&packed_offs_stream, src, src_end,
                               &lztable->offs_stream_size,
                               Min(scratch_end - scratch, offs_stream_limit),
                               false, scratch, scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
        scratch += lztable->offs_stream_size;
    }
    else
    {
        // uses the mode where distances are coded with 2 tables
        // and the transformation offs * scaling + low_bits
        offs_scaling = src[0] - 127;
        src++;

        packed_offs_stream = scratch;
        n = Kraken_DecodeBytes(&packed_offs_stream, src, src_end,
                               &lztable->offs_stream_size,
                               Min(scratch_end - scratch, offs_stream_limit),
                               false, scratch, scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
        scratch += lztable->offs_stream_size;

        if (offs_scaling != 1)
        {
            packed_offs_stream_extra = scratch;
            n = Kraken_DecodeBytes(&packed_offs_stream_extra, src, src_end, &decode_count,
                                   Min(scratch_end - scratch, offs_stream_limit),
                                   false, scratch, scratch_end);
            if (n < 0 || decode_count != lztable->offs_stream_size)
            {
                return false;
            }
            src += n;
            scratch += decode_count;
        }
    }

    // Decode packed litlen stream. It's bounded by 1/5 of dst_size.
    packed_len_stream = scratch;
    n = Kraken_DecodeBytes(&packed_len_stream, src, src_end, &lztable->len_stream_size,
                           Min(scratch_end - scratch, dst_size / 5), false,
                           scratch, scratch_end);
    if (n < 0)
    {
        return false;
    }
    src += n;
    scratch += lztable->len_stream_size;

    // Reserve memory for final dist stream
    scratch = ALIGN_POINTER(scratch, 16);
    lztable->offs_stream = (int*)scratch;
    scratch += lztable->offs_stream_size * 4;

    // Reserve memory for final len stream
    scratch = ALIGN_POINTER(scratch, 16);
    lztable->len_stream = (int*)scratch;
    scratch += lztable->len_stream_size * 4;

    if (scratch > scratch_end)
    {
        return false;
    }

    if (chunk_type <= 1)
    {
        // Decode lit stream, bounded by dst_size
        out = scratch;
        n = Kraken_DecodeBytes(&out, src, src_end, &decode_count,
                               Min(scratch_end - scratch, dst_size),
                               true, scratch, scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
        lztable->lit_stream[0] = out;
        lztable->lit_stream_size[0] = decode_count;
    }
    else
    {
        int array_count = (chunk_type == 2) ? 2 :
                          (chunk_type == 3) ? 4 : 16;
        n = Kraken_DecodeMultiArray(src, src_end, scratch, scratch_end,
                                    lztable->lit_stream, lztable->lit_stream_size,
                                    array_count, &decode_count, true, scratch,
                                    scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
    }
    scratch += decode_count;
    lztable->lit_stream_total = decode_count;

    if (src >= src_end)
    {
        return false;
    }

    if (!(src[0] & 0x80))
    {
        // Decode command stream, bounded by dst_size
        out = scratch;
        n = Kraken_DecodeBytes(&out, src, src_end, &decode_count, Min(scratch_end - scratch, dst_size),
                               true, scratch, scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
        lztable->cmd_stream = out;
        lztable->cmd_stream_size = decode_count;
        scratch += decode_count;
    }
    else
    {
        if (src[0] != 0x83)
        {
            return false;
        }
        src++;
        int multi_cmd_lens[8];
        n = Kraken_DecodeMultiArray(src, src_end, scratch, scratch_end, lztable->multi_cmd_ptr,
                                    multi_cmd_lens, 8, &decode_count, true, scratch, scratch_end);
        if (n < 0)
        {
            return false;
        }
        src += n;
        for (size_t i = 0; i < 8; i++)
        {
            lztable->multi_cmd_end[i] = lztable->multi_cmd_ptr[i] + multi_cmd_lens[i];
        }

        lztable->cmd_stream = NULL;
        lztable->cmd_stream_size = decode_count;
        scratch += decode_count;
    }

    if (dst_size > scratch_end - scratch)
    {
        return false;
    }


    return Kraken_UnpackOffsets(src, src_end, packed_offs_stream, packed_offs_stream_extra,
                                lztable->offs_stream_size, offs_scaling,
                                packed_len_stream, lztable->len_stream_size,
                                lztable->offs_stream, lztable->len_stream, 0, 0);
}



// Leviathan_ProcessLz()
template<typename Mode, bool MultiCmd>
bool Leviathan_ProcessLz(LeviathanLzTable *lzt, uint8_t *dst, uint8_t *dst_start,
                         uint8_t *dst_end, uint8_t *window_base)
{
    const uint8_t *cmd_stream = lzt->cmd_stream;
    const uint8_t *cmd_stream_end = cmd_stream + lzt->cmd_stream_size;
    const int *len_stream = lzt->len_stream;
    const int *len_stream_end = len_stream + lzt->len_stream_size;

    const int *offs_stream = lzt->offs_stream;
    const int *offs_stream_end = offs_stream + lzt->offs_stream_size;
    const byte *copyfrom;
    uint8_t *match_zone_end = (dst_end - dst_start >= 16) ? dst_end - 16 : dst_start;

    int32_t recent_offs[16];
    recent_offs[8] = recent_offs[9] = recent_offs[10] = recent_offs[11] = -8;
    recent_offs[12] = recent_offs[13] = recent_offs[14] = -8;

    size_t offset = -8;

    Mode mode(lzt, dst_start);

    uint32_t cmd_stream_left;
    const uint8_t *multi_cmd_stream[8],
    const uint8_t **cmd_stream_ptr;

    if (MultiCmd)
    {
        for (size_t i = 0; i != 8; i++)
        {
            multi_cmd_stream[i] = lzt->multi_cmd_ptr[(i - (uintptr_t)dst_start) & 7];
        }
        cmd_stream_left = lzt->cmd_stream_size;
        cmd_stream_ptr = &multi_cmd_stream[(uintptr_t)dst & 7];
        cmd_stream = *cmd_stream_ptr;
    }

    for(;;)
    {
        uint32_t cmd;

        if (!MultiCmd)
        {
            if (cmd_stream >= cmd_stream_end)
            {
                break;
            }
            cmd = *cmd_stream++;
        }
        else
        {
            if (cmd_stream_left == 0)
            {
                break;
            }
            cmd_stream_left--;
            cmd = *cmd_stream;
            *cmd_stream_ptr = cmd_stream + 1;
        }

        uint32_t offs_index = cmd >> 5;
        uint32_t matchlen = (cmd & 7) + 2;

        recent_offs[15] = *offs_stream;

        if (!mode.CopyLiterals(cmd, dst, len_stream, match_zone_end, offset))
        {
             return false;
        }

        offset = recent_offs[(size_t)offs_index + 8];

        // Permute the recent offsets table
        __m128i temp = _mm_loadu_si128((const __m128i *)&recent_offs[(size_t)offs_index + 4]);
        _mm_storeu_si128((__m128i *)&recent_offs[(size_t)offs_index + 1], _mm_loadu_si128((const __m128i *)&recent_offs[offs_index]));
        _mm_storeu_si128((__m128i *)&recent_offs[(size_t)offs_index + 5], temp);
        recent_offs[8] = (int32)offset;
        offs_stream += offs_index == 7;

        if ((uintptr_t)offset < (uintptr_t)(window_base - dst))
        {
             return false;  // offset out of bounds
        }
        copyfrom = dst + offset;

        if (matchlen == 9)
        {
            if (len_stream >= len_stream_end)
            {
                return false;  // len stream empty
            }
            matchlen = *--len_stream_end + 6;
            COPY_64(dst, copyfrom);
            COPY_64(dst + 8, copyfrom + 8);
            uint8_t *next_dst = dst + matchlen;
            if (MultiCmd)
            {
                cmd_stream = *(cmd_stream_ptr = &multi_cmd_stream[(uintptr_t)next_dst & 7]);
            }
            if (matchlen > 16)
            {
                if (matchlen > (uintptr_t)(dst_end - 8 - dst))
                {
                    return false;  // no space in buf
                }
                COPY_64(dst + 16, copyfrom + 16);
                do {
                    COPY_64(dst + 24, copyfrom + 24);
                    matchlen -= 8;
                    dst += 8;
                    copyfrom += 8;
                } while (matchlen > 24);
            }
            dst = next_dst;
        }
        else
        {
            COPY_64(dst, copyfrom);
            dst += matchlen;
            if (MultiCmd)
            {
                cmd_stream = *(cmd_stream_ptr = &multi_cmd_stream[(uintptr_t)dst & 7]);
            }
        }
    }

    // check for incorrect input
    if (offs_stream != offs_stream_end || len_stream != len_stream_end)
    {
        return false;
    }

    // copy final literals
    if (dst < dst_end)
    {
        mode.CopyFinalLiterals(dst_end - dst, dst, offset);
    }
    else if (dst != dst_end)
    {
        return false;
    }
    return true;
}



// Leviathan_ProcessLzRuns()
bool Leviathan_ProcessLzRuns(int chunk_type, byte *dst, int dst_size, int offset, LeviathanLzTable *lzt)
{
    uint8_t *dst_cur = dst + (offset == 0 ? 8 : 0);
    uint8_t *dst_end = dst + dst_size;
    uint8_t *dst_start = dst - offset;

    if (lzt->cmd_stream != NULL)
    {
        // single cmd mode
        switch (chunk_type) {
        case 0:
            return Leviathan_ProcessLz<LeviathanModeSub, false>(lzt, dst_cur, dst, dst_end, dst_start);
        case 1:
            return Leviathan_ProcessLz<LeviathanModeRaw, false>(lzt, dst_cur, dst, dst_end, dst_start);
        case 2:
            return Leviathan_ProcessLz<LeviathanModeLamSub, false>(lzt, dst_cur, dst, dst_end, dst_start);
        case 3:
            return Leviathan_ProcessLz<LeviathanModeSubAnd3, false>(lzt, dst_cur, dst, dst_end, dst_start);
        case 4:
            return Leviathan_ProcessLz<LeviathanModeO1, false>(lzt, dst_cur, dst, dst_end, dst_start);
        case 5:
            return Leviathan_ProcessLz<LeviathanModeSubAndF, false>(lzt, dst_cur, dst, dst_end, dst_start);
        }
    }
    else
    {
        // multi cmd mode
        switch (chunk_type) {
        case 0:
            return Leviathan_ProcessLz<LeviathanModeSub, true>(lzt, dst_cur, dst, dst_end, dst_start);
        case 1:
            return Leviathan_ProcessLz<LeviathanModeRaw, true>(lzt, dst_cur, dst, dst_end, dst_start);
        case 2:
            return Leviathan_ProcessLz<LeviathanModeLamSub, true>(lzt, dst_cur, dst, dst_end, dst_start);
        case 3:
            return Leviathan_ProcessLz<LeviathanModeSubAnd3, true>(lzt, dst_cur, dst, dst_end, dst_start);
        case 4:
            return Leviathan_ProcessLz<LeviathanModeO1, true>(lzt, dst_cur, dst, dst_end, dst_start);
        case 5:
            return Leviathan_ProcessLz<LeviathanModeSubAndF, true>(lzt, dst_cur, dst, dst_end, dst_start);
        }

    }
    return false;
}



// Leviathan_DecodeQuantum()
//
// Decode one 256kb big quantum block. It's divided into two 128k blocks
// internally that are compressed separately but with a shared history.
int Leviathan_DecodeQuantum(byte *dst, byte *dst_end, byte *dst_start,
                            const byte *src, const byte *src_end,
                            byte *scratch, byte *scratch_end)
{
    const byte *src_in = src;
    int mode;
    int chunkhdr; 
    int dst_count; 
    int src_used; 
    int written_bytes;

    while (dst_end - dst != 0)
    {
        dst_count = dst_end - dst;
        if (dst_count > 0x20000)
        {
            dst_count = 0x20000;
        }
        if (src_end - src < 4)
        {
            return -1;
        }
        chunkhdr = src[2] | src[1] << 8 | src[0] << 16;
        if (!(chunkhdr & 0x800000))
        {
            // Stored as entropy without any match copying.
            byte *out = dst;
            src_used = Kraken_DecodeBytes(&out, src, src_end, &written_bytes, dst_count, false, scratch, scratch_end);
            if (src_used < 0 || written_bytes != dst_count)
            {
                return -1;
            }
        }
        else
        {
            src += 3;
            src_used = chunkhdr & 0x7FFFF;
            mode = (chunkhdr >> 19) & 0xF;
            if (src_end - src < src_used)
            {
                return -1;
            }
            if (src_used < dst_count)
            {
                size_t scratch_usage = Min(Min(3 * dst_count + 32 + 0xd000, 0x6C000), scratch_end - scratch);
                if (scratch_usage < sizeof(LeviathanLzTable))
                {
                    return -1;
                }
                if (!Leviathan_ReadLzTable(mode, src, src + src_used, dst,
                                           dst_count, dst - dst_start,
                                           scratch + sizeof(LeviathanLzTable),
                                           scratch + scratch_usage,
                                           (LeviathanLzTable*)scratch))
                {
                    return -1;
                }
                if (!Leviathan_ProcessLzRuns(mode, dst, dst_count, dst - dst_start, (LeviathanLzTable*)scratch))
                {
                    return -1;
                }
            }
            else if (src_used > dst_count || mode != 0)
            {
                return -1;
            }
            else
            {
                memmove(dst, src, dst_count);
            }
        }
        src += src_used;
        dst += dst_count;
    }
    return src - src_in;
}

