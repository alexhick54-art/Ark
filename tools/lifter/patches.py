"""Per-instruction replacements applied by gen.py: guest address -> function(gen, addr) returning C code.

Each checks the instruction it replaces (Legends: Arceus 1.1.1) and leaves any other word as it was.
"""


def _orig(gen, a):
    return ' '.join(gen.render_single(gen.lift(a), a, lambda t: f'goto L_{t:x};', set()))


def terrain_range(gen, a):
    # The terrain's level-of-detail test (main+0x2471048): a patch is split into four finer ones when
    # distance - half its size < [manager+520] (40 m, set in the manager's constructor). This load of that limit also
    # multiplies it by the run-time factor (ark_terrain_range, runtime/patches.c: ARK_TERRAIN_RANGE, F7).
    if gen.word(a) != 0xbd420905:  # ldr s5, [x8, #520]
        return _orig(gen, a)
    return ('{ uint64_t ea = c->x[8] + 0x208ull; c->v[5] = LD32(ea); '
            'extern void ark_terrain_range(Cpu *); ark_terrain_range(c); }')


PATCHES = {
    0x70024713c4: terrain_range,
}
