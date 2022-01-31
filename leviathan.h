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


// complex struct
struct LeviathanModeRaw {

    const uint8_t *lit_stream;

    finline LeviathanModeRaw(LeviathanLzTable *lzt, uint8_t *dst_start) : lit_stream(lzt->lit_stream[0])
    {
    }

    finline bool CopyLiterals(uint32_t cmd, uint8_t *&dst, const int *&len_stream, uint8_t *match_zone_end, size_t last_offset)
    {
        uint32_t litlen = (cmd >> 3) & 3;
        // use cmov
        uint32_t len_stream_value = *len_stream & 0xffffff;
        const int *next_len_stream = len_stream + 1;
        len_stream = (litlen == 3) ? next_len_stream : len_stream;
        litlen = (litlen == 3) ? len_stream_value : litlen;
        COPY_64(dst, lit_stream);
        if (litlen > 8)
        {
            COPY_64(dst + 8, lit_stream + 8);
            if (litlen > 16)
            {
                COPY_64(dst + 16, lit_stream + 16);
                if (litlen > 24)
                {
                    if (litlen > match_zone_end - dst)
                    {
                        return false;  // out of bounds
                    }
                    do
                    {
                        COPY_64(dst + 24, lit_stream + 24);
                        litlen -= 8, dst += 8, lit_stream += 8;
                    } while (litlen > 24);
                }
            }
        }
        dst += litlen;
        lit_stream += litlen;
        return true;
    }

    finline void CopyFinalLiterals(uint32_t final_len, uint8_t *&dst, size_t last_offset)
    {
        if (final_len >= 64)
        {
            do
            {
                COPY_64_BYTES(dst, lit_stream);
                dst += 64, lit_stream += 64, final_len -= 64;
            } while (final_len >= 64);
        }
        if (final_len >= 8)
        {
            do
            {
                COPY_64(dst, lit_stream);
                dst += 8, lit_stream += 8, final_len -= 8;
            } while (final_len >= 8);
        }
        if (final_len > 0)
        {
            do
            {
                *dst++ = *lit_stream++;
            } while (--final_len);
        }
    }
};



// complex struct
struct LeviathanModeSub {

    const uint8_t *lit_stream;

    finline LeviathanModeSub(LeviathanLzTable *lzt, uint8_t *dst_start) : lit_stream(lzt->lit_stream[0])
    {
    }

    finline bool CopyLiterals(uint32_t cmd, uint8_t *&dst, const int *&len_stream, uint8_t *match_zone_end, size_t last_offset)
    {
        uint32_t litlen = (cmd >> 3) & 3;
        // use cmov
        uint32_t len_stream_value = *len_stream & 0xffffff;
        const int *next_len_stream = len_stream + 1;
        len_stream = (litlen == 3) ? next_len_stream : len_stream;
        litlen = (litlen == 3) ? len_stream_value : litlen;
        COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
        if (litlen > 8)
        {
            COPY_64_ADD(dst + 8, lit_stream + 8, &dst[last_offset + 8]);
            if (litlen > 16)
            {
                COPY_64_ADD(dst + 16, lit_stream + 16, &dst[last_offset + 16]);
                if (litlen > 24)
                {
                    if (litlen > match_zone_end - dst)
                    {
                        return false;  // out of bounds
                    do
                    {
                        COPY_64_ADD(dst + 24, lit_stream + 24, &dst[last_offset + 24]);
                        litlen -= 8, dst += 8, lit_stream += 8;
                    } while (litlen > 24);
                }
            }
        }
        dst += litlen;
        lit_stream += litlen;
        return true;
    }

    finline void CopyFinalLiterals(uint32_t final_len, uint8_t *&dst, size_t last_offset)
    {
        if (final_len >= 8)
        {
            do
            {
                COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
                dst += 8, lit_stream += 8, final_len -= 8;
            } while (final_len >= 8);
        }
        if (final_len > 0)
        {
            do
            {
                *dst = *lit_stream++ + dst[last_offset];
            } while (dst++, --final_len);
        }
    }
};




// complex struct
struct LeviathanModeLamSub {

    const uint8_t *lit_stream, *lam_lit_stream;

    finline LeviathanModeLamSub(LeviathanLzTable *lzt, uint8_t *dst_start)
        : lit_stream(lzt->lit_stream[0]), lam_lit_stream(lzt->lit_stream[1])
    {
    }

    finline bool CopyLiterals(uint32_t cmd, uint8_t *&dst, const int *&len_stream, uint8_t *match_zone_end, size_t last_offset)
    {
        uint32_t lit_cmd = cmd & 0x18;
        if (!lit_cmd)
        {
            return true;
        }

        uint32_t litlen = lit_cmd >> 3;
       
        // use cmov
        uint32_t len_stream_value = *len_stream & 0xffffff;
        const int *next_len_stream = len_stream + 1;
        len_stream = (litlen == 3) ? next_len_stream : len_stream;
        litlen = (litlen == 3) ? len_stream_value : litlen;

        if (litlen-- == 0)
        {
            return false; // lamsub mode requires one literal
        }

        dst[0] = *lam_lit_stream++ + dst[last_offset], dst++;

        COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
        if (litlen > 8)
        {
            COPY_64_ADD(dst + 8, lit_stream + 8, &dst[last_offset + 8]);
            if (litlen > 16)
            {
                COPY_64_ADD(dst + 16, lit_stream + 16, &dst[last_offset + 16]);
                if (litlen > 24)
                {
                    if (litlen > match_zone_end - dst)
                    {
                        return false;  // out of bounds
                    }
                    do
                    {
                        COPY_64_ADD(dst + 24, lit_stream + 24, &dst[last_offset + 24]);
                        litlen -= 8, dst += 8, lit_stream += 8;
                    } while (litlen > 24);
                }
            }
        }
        dst += litlen;
        lit_stream += litlen;
        return true;
    }

    finline void CopyFinalLiterals(uint32_t final_len, uint8_t *&dst, size_t last_offset)
    {
        dst[0] = *lam_lit_stream++ + dst[last_offset], dst++;
        final_len -= 1;

        if (final_len >= 8)
        {
            do
            {
                COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
                dst += 8, lit_stream += 8, final_len -= 8;
            } while (final_len >= 8);
        }
        if (final_len > 0)
        {
            do
            {
                *dst = *lit_stream++ + dst[last_offset];
            } while (dst++, --final_len);
        }
    }
};



// complex struct
struct LeviathanModeSubAnd3 {

    enum { NUM = 4, MASK = NUM - 1};
    const uint8_t *lit_stream[NUM];

    finline LeviathanModeSubAnd3(LeviathanLzTable *lzt, uint8_t *dst_start)
    {
        for (size_t i = 0; i != NUM; i++)
        {
            lit_stream[i] = lzt->lit_stream[(-(intptr_t)dst_start + i) & MASK];
        }
    }
    finline bool CopyLiterals(uint32_t cmd, uint8_t *&dst, const int *&len_stream, uint8_t *match_zone_end, size_t last_offset)
    {
        uint32_t lit_cmd = cmd & 0x18;

        if (lit_cmd == 0x18)
        {
            uint32_t litlen = *len_stream++ & 0xffffff;
            if (litlen > match_zone_end - dst)
            {
                return false;
            }
            while (litlen)
            {
                *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
                dst++, litlen--;
            }
        }
        else if (lit_cmd)
        {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            dst++;
            if (lit_cmd == 0x10)
            {
                *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
                dst++;
            }
        }
        return true;
    }

    finline void CopyFinalLiterals(uint32_t final_len, uint8_t *&dst, size_t last_offset)
    {
        if (final_len > 0)
        {
            do
            {
                *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            } while (dst++, --final_len);
        }
    }
};



// complex struct
struct LeviathanModeSubAndF {

    enum { NUM = 16, MASK = NUM - 1};
    const uint8_t *lit_stream[NUM];

    finline LeviathanModeSubAndF(LeviathanLzTable *lzt, uint8_t *dst_start)
    {
        for(size_t i = 0; i != NUM; i++)
        {
            lit_stream[i] = lzt->lit_stream[(-(intptr_t)dst_start + i) & MASK];
        }
    }
    finline bool CopyLiterals(uint32_t cmd, uint8_t *&dst, const int *&len_stream, uint8_t *match_zone_end, size_t last_offset)
    {
        uint32_t lit_cmd = cmd & 0x18;

        if (lit_cmd == 0x18)
        {
            uint32_t litlen = *len_stream++ & 0xffffff;
            if (litlen > match_zone_end - dst)
            {
                return false;
            }
            while (litlen)
            {
                *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
                dst++, litlen--;
            }
        }
        else if (lit_cmd)
        {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            dst++;
            if (lit_cmd == 0x10) {
                *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
                dst++;
            }
        }
        return true;
    }

    finline void CopyFinalLiterals(uint32_t final_len, uint8_t *&dst, size_t last_offset)
    {
        if (final_len > 0)
        {
            do
            {
                *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            } while (dst++, --final_len);
        }
    }
};




// complex struct
struct LeviathanModeO1 {

    const uint8_t *lit_streams[16];
    uint8_t next_lit[16];

    finline LeviathanModeO1(LeviathanLzTable *lzt, uint8_t *dst_start)
    {
        for (size_t i = 0; i != 16; i++)
        {
            uint8_t *p = lzt->lit_stream[i];
            next_lit[i] = *p;
            lit_streams[i] = p + 1;
        }
    }

    finline bool CopyLiterals(uint32_t cmd, uint8_t *&dst, const int *&len_stream, uint8_t *match_zone_end, size_t last_offset)
    {
        uint32_t lit_cmd = cmd & 0x18;

        if (lit_cmd == 0x18)
        {
            uint32_t litlen = *len_stream++;
            if ((int32)litlen <= 0)
            {
                return false;
            }
            uint context = dst[-1];
            do
            {
                size_t slot = context >> 4;
                *dst++ = (context = next_lit[slot]);
                next_lit[slot] = *lit_streams[slot]++;
            } while (--litlen);
        }
        else if (lit_cmd)
        {
            // either 1 or 2
            uint context = dst[-1];
            size_t slot = context >> 4;
            *dst++ = (context = next_lit[slot]);
            next_lit[slot] = *lit_streams[slot]++;
            if (lit_cmd == 0x10)
            {
                slot = context >> 4;
                *dst++ = (context = next_lit[slot]);
                next_lit[slot] = *lit_streams[slot]++;
            }
        }
        return true;
    }

    finline void CopyFinalLiterals(uint32_t final_len, uint8_t *&dst, size_t last_offset)
    {
        uint context = dst[-1];
        while (final_len)
        {
            size_t slot = context >> 4;
            *dst++ = (context = next_lit[slot]);
            next_lit[slot] = *lit_streams[slot]++;
            final_len--;
        }
    }
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



