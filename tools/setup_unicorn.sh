#!/bin/bash
# The generator needs Unicorn's Python bindings (jump-table recovery) and the tester needs them too.
# `pip install unicorn` is the easy way; this builds it from source when pip is not available.
# Usage: source tools/setup_unicorn.sh [dir=.deps]
set -e
D=${1:-.deps}
mkdir -p "$D"
if [ ! -d "$D/unicorn" ]; then
    git clone -q --depth 1 --branch 2.1.1 https://github.com/unicorn-engine/unicorn "$D/unicorn"
fi
cmake -S "$D/unicorn" -B "$D/unicorn/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DUNICORN_ARCH=aarch64 -DUNICORN_BUILD_TESTS=OFF >/dev/null
ninja -C "$D/unicorn/build"
export LIBUNICORN_PATH=$(cd "$D/unicorn/build" && pwd)
export PYTHONPATH=$(cd "$D/unicorn/bindings/python" && pwd)${PYTHONPATH:+:$PYTHONPATH}
echo "unicorn ready: LIBUNICORN_PATH=$LIBUNICORN_PATH"
