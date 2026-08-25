#!/bin/bash
set -euo pipefail
SRC=/mnt/d/os
DST=/tmp/cordos-src
VENV=/tmp/iconsvenv

if [ ! -x "$VENV/bin/python" ]; then
  python3 -m venv "$VENV"
  "$VENV/bin/pip" install --quiet cairosvg pillow
fi

mkdir -p "$DST"
rsync -a --delete \
  --exclude out --exclude build --exclude iso64 --exclude kbuild \
  --exclude out-new --exclude test-dir --exclude dist --exclude .git \
  --exclude iso \
  "$SRC/" "$DST/"
cd "$DST"
make ARCH=x86_64 -j"$(nproc)"
mkdir -p /mnt/d/os/dist
cp -f out/cordos.iso /mnt/d/os/dist/cordos.iso
ls -l /mnt/d/os/dist/cordos.iso
