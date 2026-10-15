// Ark headless build: just enough of SDL3's audio API for runtime/services/audio.c to compile.
// SDL_InitSubSystem fails, so the audio services run without an output device (the game still sees a working
// audio renderer; nothing is played). Real builds use SDL3 itself.
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct SDL_AudioStream SDL_AudioStream;
typedef uint32_t SDL_AudioDeviceID;
typedef enum { SDL_AUDIO_S16 = 0x8010, SDL_AUDIO_F32 = 0x8120 } SDL_AudioFormat;
typedef struct { SDL_AudioFormat format; int channels; int freq; } SDL_AudioSpec;
#define SDL_INIT_AUDIO 0x10u
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ((SDL_AudioDeviceID)0xFFFFFFFFu)
static inline bool SDL_InitSubSystem(uint32_t f) { (void)f; return false; }
static inline const char *SDL_GetError(void) { return "headless build (no SDL)"; }
static inline SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID d, const SDL_AudioSpec *s, void *cb, void *u) {
    (void)d; (void)s; (void)cb; (void)u; return 0;
}
static inline void SDL_DestroyAudioStream(SDL_AudioStream *s) { (void)s; }
static inline SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream *s) { (void)s; return 0; }
static inline bool SDL_GetAudioDeviceFormat(SDL_AudioDeviceID d, SDL_AudioSpec *s, int *frames) {
    (void)d; (void)s; if (frames) *frames = 0; return false;
}
static inline float SDL_GetAudioStreamGain(SDL_AudioStream *s) { (void)s; return 1.0f; }
static inline bool SDL_SetAudioStreamGain(SDL_AudioStream *s, float g) { (void)s; (void)g; return true; }
static inline int SDL_GetAudioStreamQueued(SDL_AudioStream *s) { (void)s; return 0; }
static inline bool SDL_PutAudioStreamData(SDL_AudioStream *s, const void *b, int n) { (void)s; (void)b; (void)n; return true; }
static inline bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *s) { (void)s; return true; }
