// Ark: RomFS mods. ARK_MODS=<folder>: files under <folder>/romfs/ replace the game's (Atmosphere's layered RomFS
// layout, so community mods drop in as they are). ARK_LOD=1/2: draw-distance presets applied to the game's own
// app_config files in memory (runtime/mods.c).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// host file replacing RomFS file `path` ("bin/...": no leading slash needed), if a mod has one
bool mods_romfs_override(const char *path, char *host, size_t size);
// edited copy of RomFS file `path` (original bytes in `data`), or NULL when no preset changes it; *out_size its size
uint8_t *mods_config_edit(const char *path, const uint8_t *data, uint64_t size, uint64_t *out_size);
