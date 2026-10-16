// Ark: RomFS mods (see mods.h).
#include "mods.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "rt.h"

static const char *skip_slash(const char *p) { while (*p == '/') p++; return p; }

bool mods_romfs_override(const char *path, char *host, size_t size) {
    const char *dir = getenv("ARK_MODS");
    if (!dir || !*dir) return false;
    snprintf(host, size, "%s/romfs/%s", dir, skip_slash(path));
    struct stat sb;
    if (stat(host, &sb) == 0 && S_ISREG(sb.st_mode)) {
        static int logged;
        if (logged++ < 64) LOG("[mods] %s replaced by %s\n", skip_slash(path), host);
        return true;
    }
    return false;
}

// ---- app_config edits. These files are FlatBuffers holding a list of entries, each a key string followed by its
// value strings ("200", "0", "500", "-10"). An edit sets value `idx` of `key`: in place when the new text fits,
// otherwise appended to the file with the one offset that points at the old string redirected.
typedef struct { const char *file, *key; int idx; const char *value; int preset; } Edit;  // preset: lowest level using it

// Each setting is "value,...,max,min" ("40,100,1": 40, at most 100). Presets: ARK_LOD=1/2/3 (draw distance; 1 and 2 are
// the community LoD mods' values, 3 is as far as the settings go) and ARK_SHADOWS=1 (preset 100 below). Values above a
// setting's max get the max raised with them. The trees' near fade (kTreeFadeDistanceNear) is left alone: the mods'
// change to it made trunks vanish close to the camera.
#define CC "field_common_config.bin"
#define AF "field_anime_framerate_config.bin"
#define SH "field_shadow_config.bin"
#define CAM "field_camera_config.bin"
static const Edit edits[] = {
    // 1: Pokemon/NPCs load further out, trees and small detail fade in further, distant models animate smoothly
    {CC, "kArea00NpcPokeLoadRadius", 0, "600", 1}, {CC, "kArea00NpcPokeLoadRadius", 1, "600", 1},
    {CC, "kArea00NpcLoadRadius", 0, "600", 1}, {CC, "kArea00NpcLoadRadius", 1, "600", 1},
    {CC, "kDetailFadeCoefficient", 0, "0.05", 1},
    {CC, "kTreeFadeDistanceFar", 0, "1000", 1}, {CC, "kTreeFadeDistanceFar", 2, "2500", 1},
    {CC, "kTreeVisibleDistance", 0, "1000", 1}, {CC, "kTreeVisibleDistance", 2, "2500", 1},
    {CC, "kDistanceOfCameraToTerrainLodPoint", 0, "21", 1},
    {CC, "kDistanceOfCameraToStreamLoadPoint", 0, "21", 1},
    {AF, "framerateLv0capacity", 0, "30", 1}, {AF, "framerateLv0capacity", 1, "30", 1},
    {AF, "framerateLv1capacity", 0, "30", 1}, {AF, "framerateLv1capacity", 1, "30", 1},
    {AF, "framerateLv2capacity", 0, "30", 1}, {AF, "framerateLv2capacity", 1, "30", 1},
    {AF, "distanceBaseValue", 0, "300", 1}, {AF, "distanceBaseValue", 1, "300", 1},
    // 2: terrain detail further out
    {CC, "kDistanceOfCameraToTerrainLodPoint", 0, "50", 2},
    {CC, "kDistanceOfCameraToStreamLoadPoint", 0, "50", 2},
    // 3: as far as it goes
    {CC, "kArea00NpcPokeLoadRadius", 0, "1000", 3}, {CC, "kArea00NpcPokeLoadRadius", 1, "1000", 3},
    {CC, "kArea00NpcLoadRadius", 0, "1000", 3}, {CC, "kArea00NpcLoadRadius", 1, "1000", 3},
    {CC, "kDetailFadeCoefficient", 0, "0.01", 3},
    {CC, "kTreeFadeDistanceFar", 0, "5000", 3}, {CC, "kTreeFadeDistanceFar", 2, "10000", 3},
    {CC, "kTreeVisibleDistance", 0, "5000", 3}, {CC, "kTreeVisibleDistance", 2, "10000", 3},
    {CC, "kDistanceOfCameraToTerrainLodPoint", 0, "100", 3},
    {AF, "framerateLv0capacity", 0, "100", 3}, {AF, "framerateLv0capacity", 1, "100", 3},
    {AF, "framerateLv1capacity", 0, "100", 3}, {AF, "framerateLv1capacity", 1, "100", 3},
    {AF, "framerateLv2capacity", 0, "100", 3}, {AF, "framerateLv2capacity", 1, "100", 3},
    {AF, "distanceBaseValue", 0, "1000", 3}, {AF, "distanceBaseValue", 1, "1000", 3},
    {CAM, "default_far", 0, "20000", 3},
    // ARK_SHADOWS=1: the shadow cascades pulled in (more of the shadow map near the player: sharper shadows)
    {SH, "cascadeShadowSplit1", 0, "0.18", 100},
    {SH, "cascadeShadowSplit2", 0, "0.2", 100},
    {SH, "cascadeShadowSplit3", 0, "0.2", 100},
    {SH, "cascadeShadowSplit4", 0, "0.2", 100},
};

typedef struct { uint8_t *d; uint64_t n, cap; } Buf;

static bool is_key(const char *s) {
    return ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) && strncmp(s, "cc0", 3) != 0;
}

// offset of the idx-th value string of key (its first character), 0 if absent. The values are written before their
// key (FlatBuffers builds back to front): the strings between the previous key and this one, in file order.
static uint64_t find_value(const Buf *b, const char *key, int idx) {
    uint64_t vals[64];
    int n = 0;
    for (uint64_t st = 4; st < b->n; st++) {
        // a string: printable bytes, NUL, preceded by its u32 length
        uint32_t len;
        memcpy(&len, b->d + st - 4, 4);
        if (!len || st + len >= b->n || b->d[st + len] != 0) continue;
        bool ok = true;
        for (uint32_t i = 0; i < len && ok; i++) ok = b->d[st + i] >= 0x20 && b->d[st + i] < 0x7f;
        if (!ok) continue;
        const char *s = (const char *)b->d + st;
        if (is_key(s)) {
            if (len == strlen(key) && !memcmp(s, key, len)) return idx < n ? vals[idx] : 0;
            n = 0;
        } else if (n < 64) {
            vals[n++] = st;
        }
        st += len;
    }
    return 0;
}

static bool set_value(Buf *b, uint64_t st, const char *v) {
    uint32_t old, nl = (uint32_t)strlen(v);
    uint64_t lenpos = st - 4;
    memcpy(&old, b->d + lenpos, 4);
    if (nl <= old) {
        memcpy(b->d + lenpos, &nl, 4);
        memset(b->d + st, 0, old);
        memcpy(b->d + st, v, nl);
        return true;
    }
    uint64_t ref = 0;
    int refs = 0;
    for (uint64_t p = 0; p + 4 <= lenpos; p += 4) {
        uint32_t u;
        memcpy(&u, b->d + p, 4);
        if (p + u == lenpos) { ref = p; refs++; }
    }
    if (refs != 1) return false;
    uint64_t at = (b->n + 3) & ~3ull, need = at + 4 + nl + 1 + 4;
    if (need > b->cap) { b->cap = need * 2; b->d = realloc(b->d, b->cap); }
    memset(b->d + b->n, 0, need - b->n);
    memcpy(b->d + at, &nl, 4);
    memcpy(b->d + at + 4, v, nl);
    b->n = (at + 4 + nl + 1 + 3) & ~3ull;
    uint32_t rel = (uint32_t)(at - ref);
    memcpy(b->d + ref, &rel, 4);
    return true;
}

static bool active(const Edit *ed, int lod, int shadows) { return ed->preset == 100 ? shadows : ed->preset <= lod; }

uint8_t *mods_config_edit(const char *path, const uint8_t *data, uint64_t size, uint64_t *out_size) {
    const char *e = getenv("ARK_LOD");
    int lod = e ? atoi(e) : 0, shadows = getenv("ARK_SHADOWS") && atoi(getenv("ARK_SHADOWS")) > 0;
    if (lod <= 0 && !shadows) return NULL;
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (!strstr(path, "app_config/")) return NULL;
    Buf b = {0};
    int done = 0;
    for (size_t i = 0; i < sizeof edits / sizeof *edits; i++) {
        const Edit *ed = &edits[i];
        if (!active(ed, lod, shadows) || strcmp(ed->file, name)) continue;
        bool superseded = false;  // (a higher preset's value for the same setting wins)
        for (size_t j = i + 1; j < sizeof edits / sizeof *edits; j++)
            superseded |= active(&edits[j], lod, shadows) && !strcmp(edits[j].file, ed->file) && !strcmp(edits[j].key, ed->key) &&
                          edits[j].idx == ed->idx;
        if (superseded) continue;
        if (!b.d) { b.cap = size * 2 + 256; b.d = malloc(b.cap); memcpy(b.d, data, size); b.n = size; }
        uint64_t st = find_value(&b, ed->key, ed->idx);
        if (!st || !set_value(&b, st, ed->value)) { LOG("[mods] %s: %s[%d] not found, left as is\n", name, ed->key, ed->idx); continue; }
        done++;
    }
    if (!b.d) return NULL;
    LOG("[mods] draw distance %d%s: %d settings changed in %s\n", lod, shadows ? " + shadows" : "", done, name);
    *out_size = b.n;
    return b.d;
}
