// mkromfs <folder> <out.bin>: build a RomFS image from a folder of game files (a hactool romfs/ extraction,
// optionally with mod files copied over it). Uses runtime/romfs_build.c (from LAS Recomp, MIT).
#include <stdio.h>
#include "romfs_build.h"

static void progress(uint64_t done, uint64_t total) {
    fprintf(stderr, "\r%llu / %llu MB", (unsigned long long)(done >> 20), (unsigned long long)(total >> 20));
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <romfs folder> <out.bin>\n", argv[0]);
        return 2;
    }
    char why[512];
    if (!romfs_build(argv[1], argv[2], progress, why, sizeof why)) {
        fprintf(stderr, "\nfailed: %s\n", why);
        return 1;
    }
    fprintf(stderr, "\nwrote %s\n", argv[2]);
    return 0;
}
