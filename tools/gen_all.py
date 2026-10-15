#!/usr/bin/env python3
"""Translate every code module of Pokemon Legends: Arceus v1.1.1 to C.

Usage: tools/gen_all.py <exefs dir> [out dir=generated] [--simd asm|c] [--only main,sdk,...]

<exefs dir> holds the files hactool's --exefsdir writes: main, sdk, subsdk0, subsdk1 (NSO files, as dumped).
The output is derived from your own dump: it stays on your machine and is git-ignored.
"""
import argparse
import os
import platform
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'lifter'))
from layout import MODULES, BUILD_ID  # noqa: E402
from nso import Nso  # noqa: E402


def find_exefs(path):
    """The folder holding main / sdk / subsdk0 / subsdk1. Accepts the folder itself, a parent with one exefs folder
    inside (e.g. game/ or game/exefs/exefs/), and files with a .nso extension; otherwise says what it found."""
    if not os.path.isdir(path):
        sys.exit(f'exefs folder not found: {os.path.abspath(path)}\n'
                 f'Put the exefs files (main, sdk, subsdk0, subsdk1, main.npdm) there, or pass EXEFS=<folder>.')
    for root, dirs, files in os.walk(path):
        if root[len(path):].count(os.sep) > 2:
            continue
        names = {f.lower(): f for f in files}
        if 'main' in names or 'main.nso' in names:
            if 'main' not in names:  # .nso extensions: link-free fix, copy under the expected names
                for m in ('main', 'sdk', 'subsdk0', 'subsdk1'):
                    src = os.path.join(root, names.get(m + '.nso', m + '.nso'))
                    dst = os.path.join(root, m)
                    if os.path.exists(src) and not os.path.exists(dst):
                        import shutil
                        shutil.copyfile(src, dst)
            if os.path.abspath(root) != os.path.abspath(path):
                print(f'using exefs files in {root}')
            return root
    listing = ', '.join(sorted(os.listdir(path))[:20]) or '(empty)'
    sys.exit(f'no "main" file in {os.path.abspath(path)} (or a folder below it). It contains: {listing}\n'
             f'Expected the exefs files as hactool --exefsdir writes them: main, sdk, subsdk0, subsdk1, main.npdm.')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exefs')
    ap.add_argument('out', nargs='?', default='generated')
    ap.add_argument('--simd', choices=('asm', 'c'),
                    default='asm' if platform.machine().lower() in ('arm64', 'aarch64') else 'c')
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    only = set(filter(None, a.only.split(',')))

    a.exefs = find_exefs(a.exefs)
    main_nso = Nso(os.path.join(a.exefs, 'main'))
    if not BUILD_ID.startswith(main_nso.build_id[:32]):
        sys.exit(f'main build id {main_nso.build_id} is not v1.1.1 ({BUILD_ID}); Ark targets v1.1.1 only')

    out = os.path.abspath(a.out)
    for name, fname, base in MODULES:
        if only and name not in only:
            continue
        src = os.path.join(os.path.abspath(a.exefs), fname)
        t = time.time()
        hle = os.path.join(HERE, 'hle_symbols.txt')
        r = subprocess.run([sys.executable, 'gen.py', name, src, hex(base), os.path.join(out, name), '--simd', a.simd,
                            '--hle', hle],
                           cwd=os.path.join(HERE, 'lifter'), stderr=subprocess.PIPE, text=True)
        summary = [l for l in r.stderr.splitlines() if 'progress' not in l]
        print('\n'.join(summary[-3:]))
        if r.returncode:
            sys.exit(f'{name}: generator failed')
        print(f'{name}: {time.time() - t:.0f}s')


if __name__ == '__main__':
    main()
