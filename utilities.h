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


bool arg_stdout;
bool arg_force;
bool arg_quiet;
bool arg_dll;
int arg_compressor = kCompressor_Kraken,
int arg_level = 4;
char arg_direction;
char *verifyfolder;


// typedef's
// XXX Need to remove this!!
typedef int WINAPI OodLZ_CompressFunc(
  int codec, uint8_t *src_buf, size_t src_len, uint8_t *dst_buf, int level,
  void *opts, size_t offs, size_t unused, void *scratch, size_t scratch_size);

typedef int WINAPI OodLZ_DecompressFunc(uint8_t *src_buf, int src_len, uint8_t *dst, size_t dst_size,
                                          int fuzz, int crc, int verbose,
                                          uint8_t *dst_base, size_t e, void *cb, void *cb_ctx, void *scratch, size_t scratch_size, int threadPhase);

OodLZ_CompressFunc *OodLZ_Compress;
OodLZ_DecompressFunc *OodLZ_Decompress;

// LoadLib()
void LoadLib() {

#if defined(_M_X64)
#define LIBNAME "oo2core_7_win64.dll"
  char COMPFUNCNAME[] = "XXdleLZ_Compress";
  char DECFUNCNAME[] = "XXdleLZ_Decompress";
  COMPFUNCNAME[0] = DECFUNCNAME[0] = 'O';
  COMPFUNCNAME[1] = DECFUNCNAME[1] = 'o';
#else
#define LIBNAME "oo2core_7_win32.dll"
  char COMPFUNCNAME[] = "_XXdleLZ_Compress@40";
  char DECFUNCNAME[] = "_XXdleLZ_Decompress@56";
  COMPFUNCNAME[1] = DECFUNCNAME[1] = 'O';
  COMPFUNCNAME[2] = DECFUNCNAME[2] = 'o';
#endif
  HINSTANCE mod = LoadLibraryA(LIBNAME);
  OodLZ_Compress = (OodLZ_CompressFunc*)GetProcAddress(mod, COMPFUNCNAME);
  OodLZ_Decompress = (OodLZ_DecompressFunc*)GetProcAddress(mod, DECFUNCNAME);
  if (!OodLZ_Compress || !OodLZ_Decompress)
    error("error loading", LIBNAME);
}

