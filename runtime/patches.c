// Ark: optional game patches, applied to guest memory after loading (Legends: Arceus 1.1.1 only; each checks the
// original value first). Recompiled code reads data from guest memory, so data patches work as on the console;
// code is compiled, so code changes are made where the runtime sees them (nvnWindowSetPresentInterval).
#include <stdint.h>
#include <stdlib.h>

#include "rt.h"

// ARK_60FPS=1: 60 fps at normal game speed. The game steps its simulation by a fixed frame time in nanoseconds
// (main+0x397E8D8, 33,333,334 = 1/30 s) and asks for present interval 2 at main+0x2AA27A8 (skipped there, see
// objects.c). ARK_60FPS=2: 60 fps with the 1/30 s step kept (the game runs twice as fast).
int g_ark_60fps;

void game_patches_apply(void) {
    const char *e = getenv("ARK_60FPS");
    g_ark_60fps = e ? atoi(e) : 0;
    if (!g_ark_60fps) return;
    uint64_t *step = (uint64_t *)(uintptr_t)(0x7000000000ull + 0x397E8D8);
    if (*step != 33333334ull) {
        LOG("[patch] 60 fps: unexpected frame step %llu (not version 1.1.1?): not applied\n", (unsigned long long)*step);
        g_ark_60fps = 0;
        return;
    }
    if (g_ark_60fps == 1) *step = 16666667ull;
    LOG("[patch] 60 fps (%s game speed)\n", g_ark_60fps == 1 ? "normal" : "double");
}
