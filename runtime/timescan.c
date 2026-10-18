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

static void report(void) {
    qsort(cand, ncand, sizeof *cand, cmp_delta);
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
    {   // ARK_WATCH=<addr>[,<len>] from frame ARK_WATCH_AT: name the functions touching it ([watch] lines)
        static int armed;
        const char *w = getenv("ARK_WATCH");
        if (w && !armed && (long)frame >= (getenv("ARK_WATCH_AT") ? atol(getenv("ARK_WATCH_AT")) : 1)) {
            extern uint64_t g_watch_lo, g_watch_hi;
            extern void watch_arm(void);
            char *end;
            g_watch_lo = strtoull(w, &end, 16);
            g_watch_hi = g_watch_lo + (*end == ',' ? strtoull(end + 1, NULL, 0) : 4);
            armed = 1;
            LOG("[timescan] watching %llx..%llx\n", (unsigned long long)g_watch_lo, (unsigned long long)g_watch_hi);
            watch_arm();
        }
    }
    if (scan_at < 0) {
        const char *e = getenv("ARK_TIMESCAN");
        scan_at = e ? atol(e) : 0;
        if (getenv("ARK_TIMESCAN_FRAMES")) nframes = atoi(getenv("ARK_TIMESCAN_FRAMES"));
    }
    if (!scan_at || (long)frame < scan_at || step > nframes) return;
    if (step == 0) collect();
    else filter(step == 1);
    if (step == nframes) report();
    step++;
}
