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



// Main
int main(int argc, char *argv[])
{

    __int64 start;
    __int64 end;
    __int64 freq;
    int argi;

    if (argc < 2 || (argi = ParseCmdLine(argc, argv)) < 0 ||
        argi >= argc ||                                         // no files
        arg_direction != 'b' && (argc - argi) > 2 ||            // too many files
        arg_direction == 't' && (argc - argi) != 2              // missing argument for verify
        )
    {
        fprintf(stderr, "ooz v7.0\n\n"
        "Usage: ooz [options] input [output]\n"
        " -c --stdout              write to stdout\n"
        " -d --decompress          decompress (default)\n"
        " -z --compress            compress (requires oo2core_7_win64.dll)\n"
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

    for (; argi < argc; argi++)
    {
        const char *curfile = argv[argi];

        int input_size;
        byte *input = load_file(curfile, &input_size);

        byte *output = NULL;
        int outbytes = 0;

        if (arg_direction == 'z')
        {
            // compress using the dll
            LoadLib();
            output = new byte[input_size + 65536];
            if (!output)
            {
                error("memory error", curfile);
            }
            *(uint64*)output = input_size;
            QueryPerformanceCounter((LARGE_INTEGER*)&start);
            outbytes = OodLZ_Compress(arg_compressor, input, input_size,
                                      output + 8, arg_level, 0, 0, 0, 0, 0);
            if (outbytes < 0)
            {
                error("compress failed", curfile);
            }
            outbytes += 8;
            QueryPerformanceCounter((LARGE_INTEGER*)&end);
            QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
            double seconds = (double)(end - start) / freq;
            if (!arg_quiet)
            {
                fprintf(stderr, "%-20s: %8d => %8ld (%.2f seconds, %.2f MB/s)\n",
                        argv[argi], input_size, outbytes, seconds,
                        input_size * 1e-6 / seconds);
            }
        }
        else
        {
            if (arg_dll)
            {
                LoadLib();
            }

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

            QueryPerformanceCounter((LARGE_INTEGER*)&start);

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

            QueryPerformanceCounter((LARGE_INTEGER*)&end);
            QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
            double seconds = (double)(end - start) / freq;

            if (!arg_quiet)
            {
                fprintf(stderr, "%-20s: %8d => %8lld (%.2f seconds, %.2f MB/s)\n",
                        argv[argi], input_size, unpacked_size, seconds,
                        unpacked_size * 1e-6 / seconds);
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

