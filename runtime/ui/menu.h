// Ark: the in-game menu hooks the Vulkan backend calls (no menu yet: runtime/ui_none.c).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL_events.h>
#ifdef __cplusplus
extern "C" {
#endif
bool menu_handle_event(const SDL_Event *e);
void menu_apply_window_requests(void *sdl_window);
void menu_render(void *cmd, uint32_t target_w, uint32_t target_h);
#ifdef __cplusplus
}
#endif
