# oozlin
A linux port of the open source project: [powzix/ooz](https://github.com/powzix/ooz).

Supports oo2ext_7_win64.dll which, at the time of this writing, can be copied from Steam's Cyberpunk 2077.

```$ ./oozlin -h
oozlin v0.1.0

Usage: oozlin [options] input [output]
 -c --stdout              write to stdout
 -d --decompress          decompress (default)
 -z --compress            compress (requires oo2ext_7_win64.dll)
 -b                       just benchmark, don't overwrite anything
 -f                       force overwrite existing file
 --dll                    decompress with the dll
 --verify                 decompress and verify that it matches output
 --verify=<folder>        verify with files in this folder
 -<1-9> --level=<-4..10>  compression level
 -m<k>                    [k|m|s|l|h] compressor selection
 --kraken --mermaid --selkie --leviathan --hydra    compressor selection

(Warning! not fuzz safe, so please trust the input)
```

#### Build

Oodle .dll must be in root directory before running build script.

```
$ ./build.sh
```

#### Uncompress (using testdata files):
```
$ ./oozlin -d testdata/xml.kraken xml.kraken.out
Library is loaded..
testdata/xml.kraken :   484282 =>  5345280 (0.00 seconds, inf MB/s) 
```

#### Compress (using any available file):
```
$ ./oozlin -z --kraken libreoffice.tar libreoffice.tar.K
Library is loaded..
libreoffice.tar     :    20480 =>     4554 (0.00 seconds, inf MB/s)
```

Note: Output filenames above were arbitrarily given. 

