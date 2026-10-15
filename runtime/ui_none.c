// Ark: LAS Recomp's in-game menu (ImGui overlay) isn't part of Ark yet; the Vulkan backend's hooks into it are no-ops.
#include <stdbool.h>
#include <stdint.h>

#include <SDL3/SDL_events.h>

bool menu_handle_event(const SDL_Event *e) { (void)e; return false; }  // false: the game gets the event
void menu_apply_window_requests(void *sdl_window) { (void)sdl_window; }
void menu_render(void *cmd, uint32_t target_w, uint32_t target_h) { (void)cmd; (void)target_w; (void)target_h; }
void menu_test_open(void) {}
void save_editor_frame(uint64_t frame) { (void)frame; }

typedef struct SDL_Window SDL_Window;
void rt_window_icon(SDL_Window *w) { (void)w; }
bool menu_input_captured(void) { return false; }  // (no overlay menu: the game always gets the controls)
const char *game_level_name(void) { return "?"; }  // (LAS logged the current map with its FPS line)
