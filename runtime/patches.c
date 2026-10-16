// Ark: optional game patches, applied to guest memory after loading (Legends: Arceus 1.1.1 only; each checks the
// original value first). Recompiled code reads data from guest memory, so data patches work as on the console;
// code is compiled, so code changes are made where the runtime sees them (nvnWindowSetPresentInterval).
#include <stdint.h>
#include <stdlib.h>

#include "rt.h"

// ARK_60FPS=1: 60 fps at normal game speed. The game steps its simulation by a fixed frame time in nanoseconds
// (main+0x397E8D8, 33,333,334 = 1/30 s, in a table of frame times for present intervals 1..5; a second copy at
// main+0x39848E0 serves other systems) and asks for present interval 2 at main+0x2AA27A8 (skipped there, see
// objects.c). ARK_60FPS=2: 60 fps with the 1/30 s step kept (the game runs twice as fast).
int g_ark_60fps;
static int sites_mask = 15;

// Run-time alternatives in the translated code (tools/lifter/variants.py): 1/30 s fixed steps in code, 1/60 s at 60 fps.
// ARK_60FPS_SITES=<mask> picks the groups (default all; 0 none), to narrow down a group that misbehaves.
int ark_variant(int group) { return g_ark_60fps == 1 && (sites_mask & group) != 0; }

void game_patches_apply(void) {
    const char *e = getenv("ARK_60FPS");
    g_ark_60fps = e ? atoi(e) : 0;
    if (!g_ark_60fps) return;
    if (getenv("ARK_60FPS_SITES")) sites_mask = (int)strtol(getenv("ARK_60FPS_SITES"), NULL, 0);
    uint64_t *step = (uint64_t *)(uintptr_t)(0x7000000000ull + 0x397E8D8);
    if (*step != 33333334ull) {
        LOG("[patch] 60 fps: unexpected frame step %llu (not version 1.1.1?): not applied\n", (unsigned long long)*step);
        g_ark_60fps = 0;
        return;
    }
    // a second copy of the same table (frame times for intervals 1..5) feeds other systems (animation, dialogue)
    uint64_t *step2 = (uint64_t *)(uintptr_t)(0x7000000000ull + 0x39848E8);
    if (g_ark_60fps == 1) {
        *step = 16666667ull;
        if (*step2 == 33333334ull && step2[-1] == 16666667ull) *step2 = 16666667ull;
        else LOG("[patch] 60 fps: second frame-time table not as expected, left alone\n");
    }
    LOG("[patch] 60 fps (%s game speed%s)\n", g_ark_60fps == 1 ? "normal" : "double",
        g_ark_60fps == 1 && sites_mask != 15 ? ", some fixed-step code sites left at 1/30 s" : "");
}
