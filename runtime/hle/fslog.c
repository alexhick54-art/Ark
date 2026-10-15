// Ark: file-open logging. Wraps the game's nn::fs::OpenFile / GetEntryType imports (calling the SDK's own code, which
// reads the RomFS / save data as usual) to report every path that fails to open, and with ARK_LOG_OPEN=1 every path
// opened. A missing file is otherwise silent: the game just gets an error result (or a null resource later on).
#include <stdlib.h>

#include "../hle.h"

static uint64_t orig_of(const char *sym, uint64_t *cache) {
    if (!*cache) *cache = loader_find_symbol(sym);
    if (!*cache) FATAL("fslog: no SDK implementation of %s\n", sym);
    return *cache;
}

// nn::fs::OpenFile(FileHandle *out, const char *path, int mode) -> Result
HLE_FUNC("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", fs_open_file_logged) {
    static uint64_t a;
    const char *path = (const char *)(uintptr_t)c->x[1];
    char copy[512];
    snprintf(copy, sizeof copy, "%s", path ? path : "(null)");
    int mode = (int)c->x[2];
    uint64_t lr = c->x[30];
    guest_call(c, orig_of("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", &a));
    c->x[30] = lr;
    uint32_t r = (uint32_t)c->x[0];
    if (r) LOG("[fs] OpenFile %s (mode %d) FAILED %#x (module %u, desc %u)\n", copy, mode, r, r & 0x1FF, (r >> 9) & 0x1FFF);
    else if (ENV("ARK_LOG_OPEN")) LOG("[fs] OpenFile %s\n", copy);
}

// nn::fs::GetEntryType(DirectoryEntryType *out, const char *path) -> Result (existence checks: logged with ARK_LOG_OPEN)
HLE_FUNC("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc", fs_entry_type_logged) {
    static uint64_t a;
    const char *path = (const char *)(uintptr_t)c->x[1];
    char copy[512];
    snprintf(copy, sizeof copy, "%s", path ? path : "(null)");
    uint64_t lr = c->x[30];
    guest_call(c, orig_of("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc", &a));
    c->x[30] = lr;
    if (ENV("ARK_LOG_OPEN")) LOG("[fs] GetEntryType %s -> %#x\n", copy, (uint32_t)c->x[0]);
}
