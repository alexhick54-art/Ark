"""Per-instruction replacements applied by gen.py: guest address -> function(gen, addr) returning C code.

Empty for now. Game-specific fixes (60fps timing, LOD, culling) will live here or in runtime hooks once the
game boots; see docs/STATUS.md.
"""
PATCHES = {}
