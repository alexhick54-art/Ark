// Ark: user settings (minimal for now; LAS Recomp's settings.c has the full set).
#pragma once
typedef struct {
    int reverb;     // audio: game reverb on
    double volume;  // audio: master gain 0..1
} Settings;
extern Settings g_settings;
