// Ark: host-side pieces LAS Recomp's services expect that Ark doesn't have (yet).
#include <stdint.h>
#include "settings.h"

Settings g_settings = {1, 1.0};

// LAS save-file features (save editor, extended flags): no-ops.
void save_editor_file_read(const char *path, uint64_t addr, int64_t off, int64_t size) {
    (void)path; (void)addr; (void)off; (void)size;
}
void flags_ext_save_read(const char *path, uint64_t addr, int64_t off, int64_t size) {
    (void)path; (void)addr; (void)off; (void)size;
}
void flags_ext_save_written(const char *path, uint64_t addr, int64_t off, int64_t size) {
    (void)path; (void)addr; (void)off; (void)size;
}

#include <stdio.h>
#include <stdlib.h>

// LAS's romfs read hook (randomizer bookkeeping): nothing to do.
void romfs_file_read_hook(const char *path) { (void)path; }

// Software keyboard text entry. LAS asks through its overlay menu; Ark has no UI yet, so the answer comes from
// ARK_KEYBOARD_TEXT (default "Ark"). NOTE: runtime/hle/swkbd.c reads ShowKeyboardArg offsets probed on LAS's SDK
// version; check them against Legends: Arceus's SDK before relying on the initial text.
int menu_keyboard_run(const char *initial, int max_len, char *out, int cap) {
    (void)initial; (void)max_len;
    const char *t = getenv("ARK_KEYBOARD_TEXT");
    snprintf(out, (size_t)cap, "%s", t ? t : "Ark");
    return 1;
}
