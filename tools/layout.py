"""Guest address layout for Pokemon Legends: Arceus v1.1.1 (shared by the generator and the runtime loader).

Guest addresses are host addresses (LAS Recomp's model): each module is mapped at a fixed base and the
recompiled C code uses absolute addresses. rtld is not used; the runtime links the modules itself.
"""
MODULES = [
    # name,      exefs file, base
    ('main',     'main',     0x7000000000),   # game (~0x4300000 bytes)
    ('multimedia', 'subsdk0', 0x7008000000),  # NintendoSDK multimedia.nss (movie/audio decode)
    ('glslc',    'subsdk1',  0x700A000000),   # NVIDIA GLSL compiler (glslc.nss)
    ('sdk',      'sdk',      0x700C000000),   # nnSdk.nss: system libraries + NVN driver
]
BUILD_ID = 'aee8f150dda1b5a838806e1a5ea6827ad9f3c51e'

if __name__ == '__main__':
    for n, f, b in MODULES:
        print(n, f, hex(b))
