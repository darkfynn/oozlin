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
#include "kraken.h"
#include "stdafx.h"



bool arg_stdout;
bool arg_force;
bool arg_quiet;
bool arg_dll;
int arg_compressor = kCompressor_Kraken;
int arg_level = 4;
char arg_direction;
char *verifyfolder;



// ParseCmdLine()
int ParseCmdLine(int argc, char *argv[])
{
    int i;
    // parse command line
    for (i = 1; i < argc; i++)
    {
        char *s = argv[i];
        char c;
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




// Main
int main(int argc, char *argv[])
{

    void *oodleLib;
    clock_t start;
    clock_t end;
    int argi;

    if (argc < 2 || (argi = ParseCmdLine(argc, argv)) < 0 ||
        argi >= argc ||                                         // no files
        arg_direction != 'b' && (argc - argi) > 2 ||            // too many files
        arg_direction == 't' && (argc - argi) != 2              // missing argument for verify
        )
    {
        fprintf(stderr, "oozlin v0.1.0\n\n"
        "Usage: oozlin [options] input [output]\n"
        " -c --stdout              write to stdout\n"
        " -d --decompress          decompress (default)\n"
        " -z --compress            compress (requires oo2ext_7_win64.dll)\n"
        " -b                       just benchmark, don't overwrite anything\n"
        " -f                       force overwrite existing file\n"
        " --dll                    decompress with the dll\n"
        " --verify                 decompress and verify that it matches output\n"
        " --verify=<folder>        verify with files in this folder\n"
        " -<1-9> --level=<-4..10>  compression level\n"
        " -m<k>                    [k|m|s|l|h] compressor selection\n"
        " --kraken --mermaid --selkie --leviathan --hydra    compressor selection\n\n"
        "(Warning! not fuzz safe, so please trust the input)\n"
        );
        return 1;
    }

    bool write_mode = (argi + 1 < argc) && (arg_direction != 't' && arg_direction != 'b');

    if (!arg_force && write_mode)
    {
        struct stat sb;

        if (stat(argv[argi + 1], &sb) >= 0)
        {
            fprintf(stderr, "file %s already exists, skipping.\n", argv[argi + 1]);
            return 1;
        }
    }

    int nverify = 0;

    // load linoodle lib
    oodleLib = dlopen("libs/liblinoodle.so", RTLD_LAZY);
    if (oodleLib == nullptr)
    {
        fprintf(stderr, "Can't load library: %s\n", dlerror());
    }
    else
    {
        fprintf(stdout, "Library is loaded..\n");
    }

    // reset errors
    dlerror();

    // load symbols
    auto OodLZ_Compress   = reinterpret_cast<OodleLZ_CompressFunc>(dlsym(oodleLib, "OodleLZ_Compress"));
    auto OodLZ_Decompress = reinterpret_cast<OodleLZ_DecompressFunc>(dlsym(oodleLib, "OodleLZ_Decompress"));
    if (!OodLZ_Compress || !OodLZ_Decompress)
    {
        error("error loading", LIBNAME);
    }


    for (; argi < argc; argi++)
    {
        const char *curfile = argv[argi];

        int input_size;
        byte *input = load_file(curfile, &input_size);

        byte *output = NULL;
        size_t outbytes = 0;


        if (arg_direction == 'z')
        {
            // compress using the .so wrapped dll
            output = new byte[input_size + 65536];
            if (!output)
            {
                error("memory error", curfile);
            }
            *(uint64*)output = input_size;
            start = clock();
            outbytes = OodLZ_Compress(arg_compressor, input, input_size,
                                      output + 8, arg_level, 0, 0, 0, 0, 0);
            if (outbytes < 0)
            {
                error("compress failed", curfile);
            }
            outbytes += 8;
            end = clock();
            double seconds = (double)(end - start) / (int64_t)CLOCKS_PER_SEC;
            if (!arg_quiet)
            {
                fprintf(stderr, "%-20s: %8d => %8ld (%.6f seconds, %.6f MB/s)\n",
                        argv[argi], input_size, outbytes, seconds,
                        (input_size * 1e-6) / seconds);
            }
        }
        else
        {
            // stupidly attempt to autodetect if file uses 4-byte or 8-byte header,
            // the previous version of this tool wrote a 4-byte header.
            int hdrsize = *(uint64*)input >= 0x10000000000 ? 4 : 8;

            uint64_t unpacked_size = (hdrsize == 8) ? *(uint64_t*)input : *(uint32_t*)input;
            if (unpacked_size > (hdrsize == 4 ? 52*1024*1024 : 1024 * 1024 * 1024))
            {
                error("file too large", curfile);
            }

            output = new byte[unpacked_size + SAFE_SPACE];

            if (!output)
            {
                error("memory error", curfile);
            }

            start = clock();
            if (arg_dll)
            {
                outbytes = OodLZ_Decompress(input + hdrsize, input_size - hdrsize, output, unpacked_size, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            else
            {
                outbytes = Kraken_Decompress(input + hdrsize, input_size - hdrsize, output, unpacked_size);
            }

            if (outbytes != unpacked_size)
            {
                error("decompress error", curfile);
            }

            end = clock();
            double seconds = (double)(end - start) / (int64_t)CLOCKS_PER_SEC;
            if (!arg_quiet)
            {
                fprintf(stderr, "%-20s: %8d => %8lld (%.6f seconds, %.6f MB/s)\n",
                        argv[argi], input_size, unpacked_size, seconds,
                        (unpacked_size * (float)1e-6) / seconds);
            }
        }

        if (verifyfolder)
        {
            // Verify against the file in verifyfolder with the same basename excluding extension
            char buf[1024];
            const char *basename = curfile;
            for(const char *s = curfile; *s; s++)
            {
                if (*s == '/' || *s == '\\')
                {
                    basename = s + 1;
                }
            }

            const char *ext = strrchr(basename, '.');
            snprintf(buf, sizeof(buf), "%s/%.*s", verifyfolder,
                     ext ? (ext - basename) : strlen(basename), basename);

            if (!Verify(buf, output, outbytes, curfile))
            {
                return 1;
            }

            nverify++;
        }

        if (arg_stdout && arg_direction != 't' && arg_direction != 'b')
        {
            fwrite(output, 1, outbytes, stdout);
        }

        if (write_mode)
        {
            if (arg_direction == 't')
            {
                if (!Verify(argv[argi + 1], output, outbytes, curfile))
                {
                    return 1;
                }
                fprintf(stderr, "%s: Verify OK\n", curfile);
            }
            else
            {
                FILE *f = fopen(argv[argi + 1], "wb");
                if (!f)
                {
                    error("file open for write error", argv[argi + 1]);
                }
                if (fwrite(output, 1, outbytes, f) != outbytes)
                {
                    error("file write error", argv[argi + 1]);
                }
                fclose(f);
            }
            break;
        }
        delete[] input;
        delete[] output;
    }

    if (nverify)
    {
        fprintf(stderr, "%d files verified OK!\n", nverify);
    }

    return 0;
}

