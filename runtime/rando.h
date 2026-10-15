// Ark: LAS Recomp's services/fs.c has randomizer hooks; Ark has no randomizer, so they are no-ops.
#pragma once
#include <stdbool.h>
#include <stddef.h>
static inline const char *rando_seed_dir(void) { return NULL; }
static inline bool rando_romfs_override(const char *path, char *host, size_t size) {
    (void)path; (void)host; (void)size;
    return false;
}
