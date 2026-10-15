// Ark: where the runtime keeps its files. For now the working directory is the data root (ark.ini, save/,
// cache/). LAS Recomp's paths.c has the full data-root logic for packaged builds.
#pragma once
typedef struct SDL_Window SDL_Window;
void rt_window_icon(SDL_Window *w);  // (no icon yet)
