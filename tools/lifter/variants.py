"""Ark: instructions with a run-time alternative, chosen by the runtime (ark_variant, runtime/patches.c).

VARIANTS[module][offset] = (original word, alternative word, group). gen.py emits both and picks with
ark_variant(group); a site whose word differs from `original` (another game version) is left alone.

Legends: Arceus 1.1.1, ARK_60FPS=1: code that steps by a fixed 1/30 s (0x3D088889, built with MOVZ 0x8889 + MOVK
0x3D08) instead of the frame time, switched to 1/60 s (MOVK 0x3C88). Groups (ARK_60FPS_SITES, a mask; all by default):
  1  per-frame timers and integrations (t += 1/30, position += velocity * 1/30)
  2  update functions' fixed-step fallback (flag ? frame time : 1/30)
  4  objects' default time step, stored by their constructors
  8  1/30 passed as the time step argument, or returned by a getter

Group 32: the frame number the shaders get (main+0x16B668, scene constants: float(frame count)) counts at half rate,
`scvtf s4, s4` -> `scvtf s4, s4, #1`, so effects animated by frame number in shaders (water, fire) keep their speed.

Group 16 (ARK_LOD=3, or ARK_LOD0=1): models always at their full-detail level. The per-model level-of-detail update
(main+0x1ACD904) switches on the model's LOD mode (+496): 0 and 4 (what the game's SetLodLevelZero command sets) pick
level 0, 1-3 choose by distance. Its load of the mode becomes `mov w8, #4`.

Left at 1/30: conversions between frames and seconds of 30 fps data (main+0x8EEDB4, main+0x191D2B8), and vector code
that only tests against the value.
"""
VARIANTS = {
    'main': {
        0x13b33a0: (0x72a7a108, 0x72a79108, 1),
        0x167ff10: (0x72a7a108, 0x72a79108, 1),
        0x16832c4: (0x72a7a108, 0x72a79108, 1),
        0x16832e8: (0x72a7a108, 0x72a79108, 1),
        0x16836d4: (0x72a7a108, 0x72a79108, 1),
        0x1982934: (0x72a7a108, 0x72a79108, 1),
        0x1983764: (0x72a7a108, 0x72a79108, 8),
        0x198ec70: (0x72a7a108, 0x72a79108, 1),
        0x1a0baf4: (0x72a7a108, 0x72a79108, 1),
        0x1a0bee8: (0x72a7a108, 0x72a79108, 1),
        0x1a0da24: (0x72a7a108, 0x72a79108, 1),
        0x1a0eea4: (0x72a7a108, 0x72a79108, 1),
        0x1a0f308: (0x72a7a108, 0x72a79108, 8),
        0x1a0f49c: (0x72a7a116, 0x72a79116, 8),
        0x1a0f560: (0x72a7a108, 0x72a79108, 1),
        0x1a17b14: (0x72a7a108, 0x72a79108, 1),
        0x1a17f40: (0x72a7a108, 0x72a79108, 1),
        0x1a19108: (0x72a7a108, 0x72a79108, 8),
        0x1a196c8: (0x72a7a108, 0x72a79108, 8),
        0x1b48a4c: (0x72a7a108, 0x72a79108, 1),
        0x1b48a98: (0x72a7a108, 0x72a79108, 1),
        0x1b49008: (0x72a7a108, 0x72a79108, 1),
        0x1b493f8: (0x72a7a108, 0x72a79108, 1),
        0x1c5d38c: (0xf2a7a108, 0xf2a79108, 4),
        0x1d1b7d0: (0x72a7a108, 0x72a79108, 1),
        0x1d5d688: (0x72a7a108, 0x72a79108, 8),
        0x257ac34: (0x72a7a108, 0x72a79108, 1),
        0x257c9f8: (0x72a7a108, 0x72a79108, 1),
        0x257ce3c: (0x72a7a108, 0x72a79108, 1),
        0x2795234: (0x72a7a108, 0x72a79108, 2),
        0x29faf90: (0x72a7a108, 0x72a79108, 1),
        0x2aefb28: (0xf2a7a109, 0xf2a79109, 4),
        0x2af25f8: (0x72a7a108, 0x72a79108, 2),
        0x2f3e8c8: (0x72a7a10a, 0x72a7910a, 4),
        0x2f3ec18: (0xf2a7a109, 0xf2a79109, 4),
        0x2fe9660: (0xf2a7a109, 0xf2a79109, 4),
        0x3002a08: (0x72a7a109, 0x72a79109, 4),
        0x30143a0: (0x72a7a115, 0x72a79115, 4),
        0x3014d28: (0x72a7a115, 0x72a79115, 4),
        0x301526c: (0xf2a7a104, 0xf2a79104, 4),
        0x3018b94: (0xf2a7a105, 0xf2a79105, 4),
        0x301e520: (0xf2a7a109, 0xf2a79109, 4),
        0x16b668: (0x5e21d884, 0x5f3fe484, 32),  # scvtf s4, s4 -> scvtf s4, s4, #1 (shader frame number / 2)
        0x1acd924: (0xb941f2a8, 0x52800088, 16),  # ldr w8, [x21, #496] -> mov w8, #4 (LOD level 0)
    },
}
