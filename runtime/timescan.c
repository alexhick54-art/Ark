// Ark (debug): find per-frame time accumulators that don't follow the frame time. ARK_TIMESCAN=<frame>: at that
// presented frame, every float in the game's mapped memory (svcMapPhysicalMemory regions) between 1e-3 and 1e6 is
// noted; over the next ARK_TIMESCAN_FRAMES (default 4) frames, the ones that change by the same amount every frame
// stay. Logged: how many advance by each step (1/30 s and 2 frames are the 30 fps leftovers at 60 fps), and the
// addresses of the first ones for each step. ARK_WATCH=<addr> then names the functions touching one (dispatch.c).
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "rt.h"

typedef struct { uint64_t addr; float last, delta; } Cand;
static Cand *cand;
static size_t ncand, capcand;
static long scan_at = -1;
static int nframes = 4, step;

static void collect(void) {
    uint64_t a[256], s[256], gen;
    int n = kernel_phys_regions(a, s, 256, &gen);
    ncand = 0;
    for (int r = 0; r < n; r++) {
        const uint32_t *w = (const uint32_t *)(uintptr_t)a[r];
        for (uint64_t i = 0; i < s[r] / 4; i++) {
            if (!w[i]) continue;
            float f;
            memcpy(&f, &w[i], 4);
            if (!(f > 1e-3f && f < 1e6f)) continue;
            if (ncand == capcand) { capcand = capcand ? capcand * 2 : (1u << 20); cand = realloc(cand, capcand * sizeof *cand); }
            cand[ncand++] = (Cand){a[r] + i * 4, f, 0};
        }
    }
    LOG("[timescan] frame scan: %zu candidate floats in %d regions\n", ncand, n);
}

static void filter(bool first) {
    size_t k = 0;
    for (size_t i = 0; i < ncand; i++) {
        Cand c = cand[i];
        float f;
        memcpy(&f, (const void *)(uintptr_t)c.addr, 4);
        float d = f - c.last;
        if (!(fabsf(d) > 1e-6f) || !isfinite(f)) continue;
        if (!first && fabsf(d - c.delta) > fmaxf(1e-6f, fabsf(c.delta) * 1e-3f)) continue;
        cand[k++] = (Cand){c.addr, f, first ? d : c.delta};
    }
    ncand = k;
}

static int cmp_delta(const void *a, const void *b) {
    float x = ((const Cand *)a)->delta, y = ((const Cand *)b)->delta;
    return x < y ? -1 : x > y;
}

// ARK_TIMESCAN_WATCH=<step>,<step>,...: after the report, watch the first few addresses of each listed step's
// group (ARK_WATCH_FRAMES frames, default 3) so the functions writing them are named too.
static void watch_groups(void) {
    const char *e = getenv("ARK_TIMESCAN_WATCH");
    if (!e) return;
    extern void watch_add(uint64_t, uint64_t), watch_arm(void);
    for (const char *p = e; *p;) {
        char *end;
        float want = strtof(p, &end);
        if (end == p) break;
        int n = 0, per = getenv("ARK_TIMESCAN_WATCH_PER") ? atoi(getenv("ARK_TIMESCAN_WATCH_PER")) : 3;
        uint64_t lastpg = 0;
        for (size_t i = 0; i < ncand && n < per; i++) {
            if (fabsf(cand[i].delta - want) > fmaxf(1e-6f, fabsf(want) * 2e-3f)) continue;
            uint64_t pg = cand[i].addr >> 12;
            if (pg == lastpg) continue;  // spread over objects
            lastpg = pg;
            watch_add(cand[i].addr, cand[i].addr + 4);
            n++;
        }
        p = *end == ',' ? end + 1 : end;
    }
    watch_arm();
}

static void report(void) {
    qsort(cand, ncand, sizeof *cand, cmp_delta);
    FILE *f = fopen("timescan.txt", "w");
    if (f) {
        for (size_t i = 0; i < ncand; i++)
            fprintf(f, "%g %llx %g\n", cand[i].delta, (unsigned long long)cand[i].addr, cand[i].last);
        fclose(f);
    }
    LOG("[timescan] %zu floats advanced by the same step for %d frames; by step:\n", ncand, nframes);
    for (size_t i = 0; i < ncand;) {
        size_t j = i;
        while (j < ncand && fabsf(cand[j].delta - cand[i].delta) <= fmaxf(1e-6f, fabsf(cand[i].delta) * 1e-3f)) j++;
        LOG("[timescan]   step %-12g x%-6zu", cand[i].delta, j - i);
        for (size_t k = i; k < j && k < i + 8; k++) LOG(" %llx=%g", (unsigned long long)cand[k].addr, cand[k].last);
        LOG("\n");
        i = j;
    }
}

void timescan_frame(uint64_t frame) {
    {   // ARK_WATCH=<addr>[:<len>],... from frame ARK_WATCH_AT: name the functions touching them ([watch] lines),
        // for ARK_WATCH_FRAMES frames (default 3)
        static long armed_at = -1;
        extern void watch_add(uint64_t, uint64_t), watch_arm(void), watch_disarm(void);
        const char *w = getenv("ARK_WATCH");
        if (w && armed_at < 0 && (long)frame >= (getenv("ARK_WATCH_AT") ? atol(getenv("ARK_WATCH_AT")) : 1)) {
            for (const char *p = w; *p;) {
                char *end;
                uint64_t lo = strtoull(p, &end, 16), len = 4;
                if (end == p) break;
                if (*end == ':') len = strtoull(end + 1, &end, 0);
                watch_add(lo, lo + len);
                LOG("[timescan] watching %llx..%llx\n", (unsigned long long)lo, (unsigned long long)(lo + len));
                p = *end == ',' ? end + 1 : end;
            }
            armed_at = (long)frame;
            watch_arm();
        }
        extern long watch_armed_frame;
        long since = watch_armed_frame;
        long nwf = getenv("ARK_WATCH_FRAMES") ? atol(getenv("ARK_WATCH_FRAMES")) : 3;
        if (since >= 0 && (long)frame == since + nwf) { watch_armed_frame = -1; watch_disarm(); LOG("[timescan] watch off\n"); }
    }
    if (scan_at < 0) {
        const char *e = getenv("ARK_TIMESCAN");
        scan_at = e ? atol(e) : 0;
        if (getenv("ARK_TIMESCAN_FRAMES")) nframes = atoi(getenv("ARK_TIMESCAN_FRAMES"));
    }
    if (!scan_at || (long)frame < scan_at || step > nframes) return;
    if (step == 0) collect();
    else filter(step == 1);
    if (step == nframes) { report(); watch_groups(); }
    step++;
}
