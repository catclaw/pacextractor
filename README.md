To compile (On MSYS2 or MinGW):

gcc -Wall -O2 -o pacextractor.exe pacextractor.c

Debug build:
gcc -Wall -g -o pacextractor.exe pacextractor.c

Included .exe file is x64 and I use some functions that requires x64, so I doubt it will work on x86 (32-bit).

The code is optimized for speed, so I'd recommend extracting on an SSD or NVMe.
