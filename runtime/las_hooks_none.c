// Ark: LAS Recomp's renderer and input layer call into Link's Awakening features (widescreen room framing, pot glow,
// randomizer items and pets, scripted test input). Legends: Arceus has none of them, so these are their neutral
// versions: no tint, no crop, no stretch, nothing injected. (Real widescreen for PLA would replace the widescreen_*
// ones.)
#include <stdbool.h>
#include <stdint.h>

// ---- tints (renderer: per-draw colour changes)
void escape_bottle_tint(int kind, const float *ui, uint32_t ui_size, float tint[8]) {
    (void)kind; (void)ui; (void)ui_size; (void)tint;
}
bool pot_glow_active(void) { return false; }
bool pot_level_active(void) { return false; }
void pot_gold_tint(float tint[8]) { (void)tint; }
bool pot_instance_gold(const float *m) { (void)m; return false; }
bool pot_tint(const float *m, float tint[8], uint64_t mesh) { (void)m; (void)tint; (void)mesh; return false; }
bool pets_hide_item_cursor(void) { return false; }

// ---- input
uint64_t pets_menu_input(uint64_t b, int32_t st[4]) { (void)st; return b; }  // buttons unchanged
void input_note_rstick(int32_t ry) { (void)ry; }
uint64_t g_test_buttons, g_test_buttons_until;  // scripted test input (none)
int32_t g_test_lx, g_test_ly;
uint64_t g_test_stick_until;

// ---- per-frame hooks
char g_window_shot_request[256];  // LAS test shots requested by game hooks (none; LARECOMP_SHOT still works)
void reboot_test_frame(uint64_t frame) { (void)frame; }
void warp_frame(uint64_t frame) { (void)frame; }

// ---- widescreen: off (the game's own 16:9 image, letterboxed by the presenter as needed)
bool widescreen_allowed(void) { return false; }
void widescreen_backdrop_seen(uint64_t hash) { (void)hash; }
float widescreen_effect_stretch(uint64_t fs_hash) { (void)fs_hash; return 1.0f; }
void widescreen_frame_end(uint64_t frame) { (void)frame; }
void widescreen_present_crop(uint64_t frame, double shape, float crop[4], double *vx, double *vy, double *vw, double *vh) {
    (void)frame; (void)shape; (void)vx; (void)vy; (void)vw; (void)vh;
    crop[0] = crop[1] = 0;
    crop[2] = crop[3] = 1;
}
void widescreen_scene_ubo(const float *u, uint64_t size) { (void)u; (void)size; }
void widescreen_set_factor(float f) { (void)f; }
