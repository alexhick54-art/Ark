// Ark: sampling profiler. LARECOMP_SAMPLE=<seconds> samples every guest thread's host pc about 500 times a second and
// logs, every <seconds>, where the time went: guest functions as f_<addr> with their module offset (symbolize against
// the game's NSOs), other code as a library name or ark+<offset> (addr2line -fe ark <offset>). A thread asleep in the
// guest kernel counts as "(guest wait)". Works on Linux (SIGPROF) and Windows (SuspendThread / GetThreadContext).
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kernel.h"
#include "platform.h"

const char *rt_host_fn_name(uint64_t pc, uint64_t *offset);

#define NKEY 16384
typedef struct { char name[48]; uint32_t count, by_thread[32]; } Entry;
static Entry tab[NKEY];
static uint32_t total, thread_total[32];

static Entry *entry(const char *name) {
    uint64_t h = 1469598103934665603ull;
    for (const char *p = name; *p; p++) { h ^= (uint8_t)*p; h *= 1099511628211ull; }
    for (uint32_t i = 0, s = (uint32_t)h & (NKEY - 1); i < NKEY; i++, s = (s + 1) & (NKEY - 1)) {
        if (!tab[s].name[0]) { snprintf(tab[s].name, sizeof tab[s].name, "%s", name); return &tab[s]; }
        if (!strcmp(tab[s].name, name)) return &tab[s];
    }
    return NULL;
}

static void module_of(uint64_t a, char *out, size_t n) {
    static const struct { const char *name; uint64_t base, size; } m[] = {
        {"main", 0x7000000000ull, 0x8000000}, {"multimedia", 0x7008000000ull, 0x2000000},
        {"glslc", 0x700A000000ull, 0x2000000}, {"sdk", 0x700C000000ull, 0x2000000}};
    for (int i = 0; i < 4; i++)
        if (a >= m[i].base && a < m[i].base + m[i].size) { snprintf(out, n, "%s+%llx", m[i].name, (unsigned long long)(a - m[i].base)); return; }
    snprintf(out, n, "?");
}

static int cmp_count(const void *a, const void *b) {
    const Entry *x = *(Entry *const *)a, *y = *(Entry *const *)b;
    return x->count < y->count ? 1 : x->count > y->count ? -1 : 0;
}

static void report(void) {
    if (!total) return;
    static Entry *sorted[NKEY];
    int n = 0;
    for (int i = 0; i < NKEY; i++) if (tab[i].count) sorted[n++] = &tab[i];
    qsort(sorted, n, sizeof *sorted, cmp_count);
    Entry *w = entry("(guest wait)");
    uint32_t busy = total - (w ? w->count : 0);
    if (!busy) busy = 1;
    LOG("[sample] %u samples, %u not in a guest wait (the percentages below); per thread:", total, busy);
    for (int t = 0; t < 32; t++) if (thread_total[t] > (w ? w->by_thread[t] : 0)) LOG(" t%d %u", t, thread_total[t] - (w ? w->by_thread[t] : 0));
    LOG("\n");
    for (int i = 0, shown_n = 0; i < n && shown_n < 40; i++) {
        Entry *e = sorted[i];
        if (e == w) continue;
        shown_n++;
        char mod[48] = "";
        if (!strncmp(e->name, "f_", 2)) module_of(strtoull(e->name + 2, NULL, 16), mod, sizeof mod);
        LOG("[sample] %5.1f%% %-28s %-22s", 100.0 * e->count / busy, e->name, mod);
        int shown = 0;
        for (int t = 0; t < 32 && shown < 4; t++)
            if (e->by_thread[t] * 10 >= e->count) { LOG(" t%d:%u", t, e->by_thread[t]); shown++; }
        LOG("\n");
    }
    memset(tab, 0, sizeof tab);
    memset(thread_total, 0, sizeof thread_total);
    total = 0;
}

static void *sampler(void *arg) {
    uint64_t period = (uint64_t)(uintptr_t)arg * 1000000000ull, last = plat_now_ns();
    uint64_t exe = (uint64_t)(uintptr_t)rt_host_fn_name;  // (any address in the executable: offsets are relative to it)
    for (;;) {
        usleep(2000);
        KThread *ts[64];
        int n = kernel_live_threads(ts, 64);
        for (int i = 0; i < n; i++) {
            KThread *t = ts[i];
            unsigned tid = (unsigned)(t->id & 31);
            char name[48];
            if (t->wait_kind != WAIT_NONE || t->in_sync_wait) {
                snprintf(name, sizeof name, "(guest wait)");
            } else {
                uint64_t pc = 0, off = 0;
                if (!plat_thread_pc(t->cpu_clock, t->host, &pc)) continue;
                const char *s = rt_host_fn_name(pc, &off);
                if (s && off > 0x40000) s = NULL;  // (runtime code past the last guest function)
                if (!s) s = plat_symbol(pc, &off);
                if (s) snprintf(name, sizeof name, "%s", s);
                else snprintf(name, sizeof name, "ark%+lld", (long long)(pc - exe));
            }
            Entry *e = entry(name);
            if (!e) continue;
            e->count++;
            e->by_thread[tid]++;
            thread_total[tid]++;
            total++;
        }
        uint64_t now = plat_now_ns();
        if (now - last >= period) { last = now; report(); }
    }
    return NULL;
}

void sampler_start(void) {
    const char *e = getenv("LARECOMP_SAMPLE");
    if (!e || atoi(e) <= 0) return;
    pthread_t th;
    pthread_create(&th, NULL, sampler, (void *)(uintptr_t)atoi(e));
    LOG("[sample] sampling guest threads, report every %d s\n", atoi(e));
}
