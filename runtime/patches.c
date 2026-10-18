// Ark: optional game patches, applied to guest memory after loading (Legends: Arceus 1.1.1 only; each checks the
// original value first). Recompiled code reads data from guest memory, so data patches work as on the console;
// code is compiled, so code changes are made where the runtime sees them (nvnWindowSetPresentInterval).
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rt.h"

// ARK_60FPS=1: 60 fps at normal game speed. The game steps its simulation by a fixed frame time in nanoseconds
// (main+0x397E8D8, 33,333,334 = 1/30 s, in a table of frame times for present intervals 1..5; a second copy at
// main+0x39848E0, below) and asks for present interval 2 at main+0x2AA27A8 (skipped there, see
// objects.c). ARK_60FPS=2: 60 fps with the 1/30 s step kept (the game runs twice as fast).
int g_ark_60fps;
static int sites_mask = 15;

// Run-time alternatives in the translated code (tools/lifter/variants.py): 1/30 s fixed steps in code, 1/60 s at 60 fps.
// ARK_60FPS_SITES=<mask> picks the groups (default all; 0 none), to narrow down a group that misbehaves.
static int force_lod0;
int ark_variant(int group) {
    if (group == 16) return force_lod0;
    return g_ark_60fps == 1 && (sites_mask & group) != 0;
}

void game_patches_apply(void) {
    // ARK_LOD=3 (or ARK_LOD0=1): every model at its full-detail level (tools/lifter/variants.py group 16)
    force_lod0 = (getenv("ARK_LOD") && atoi(getenv("ARK_LOD")) >= 3) || (getenv("ARK_LOD0") && atoi(getenv("ARK_LOD0")) != 0);
    if (getenv("ARK_LOD0") && atoi(getenv("ARK_LOD0")) == 0) force_lod0 = 0;
    if (force_lod0) LOG("[patch] models at full detail (no level of detail)\n");
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
    // The second copy of the table (main+0x39848E0) is read in one place, main+0x2B43450: game-frame steps =
    // measured frame time / table[interval]. The measured time already follows the first table, so this one stays
    // at 1/30 s (0.5 steps a frame at 60 fps); halving it too ran those animations at double speed.
    // ARK_60FPS_TABLE2=1 halves it anyway (for comparison).
    uint64_t *step2 = (uint64_t *)(uintptr_t)(0x7000000000ull + 0x39848E8);
    if (g_ark_60fps == 1) {
        *step = 16666667ull;
        if (getenv("ARK_60FPS_TABLE2") && atoi(getenv("ARK_60FPS_TABLE2")) && *step2 == 33333334ull) *step2 = 16666667ull;
    }
    LOG("[patch] 60 fps (%s game speed%s)\n", g_ark_60fps == 1 ? "normal" : "double",
        g_ark_60fps == 1 && sites_mask != 15 ? ", some fixed-step code sites left at 1/30 s" : "");
}

// Debug: instructions named in ARK_GEN_PROBES when the code was generated call this first. From presented frame
// ARK_PROBE_AT (default 1), the first ARK_PROBE_N (default 6) visits of each are logged with the registers.
void ark_probe(Cpu *c, uint64_t pc) {
    extern uint64_t renderer_present_count(void);
    static long at = -1, maxn;
    static struct { uint64_t pc; long n; } seen[64];
    static int nseen;
    if (at < 0) {
        at = getenv("ARK_PROBE_AT") ? atol(getenv("ARK_PROBE_AT")) : 1;
        maxn = getenv("ARK_PROBE_N") ? atol(getenv("ARK_PROBE_N")) : 6;
    }
    if ((long)renderer_present_count() < at) return;
    int i = 0;
    while (i < nseen && seen[i].pc != pc) i++;
    if (i == nseen) { if (nseen == 64) return; seen[nseen++].pc = pc; }
    if (seen[i].n++ >= maxn) return;
    double d[4];
    float f[4];
    for (int k = 0; k < 4; k++) { uint64_t lo = (uint64_t)c->v[k]; memcpy(&d[k], &lo, 8); memcpy(&f[k], &lo, 4); }
    LOG("[probe] %llx frame %llu x0 %llx x1 %llx x8 %llx x9 %llx x19 %llx x20 %llx lr %llx | v0 %llx (%g) v1 %llx v2 %llx v3 %llx\n",
        (unsigned long long)pc, (unsigned long long)renderer_present_count(), (unsigned long long)c->x[0],
        (unsigned long long)c->x[1], (unsigned long long)c->x[8], (unsigned long long)c->x[9],
        (unsigned long long)c->x[19], (unsigned long long)c->x[20], (unsigned long long)c->x[30],
        (unsigned long long)(uint64_t)c->v[0], d[0], (unsigned long long)(uint64_t)c->v[1],
        (unsigned long long)(uint64_t)c->v[2], (unsigned long long)(uint64_t)c->v[3]);
}
