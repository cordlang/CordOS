#!/usr/bin/env bash
# CordOS — raw persist disk for on-disk NosFS (NOSF at LBA 2048).
# Usage: scripts/mkpersist.sh [out_img] [size_mib]
# Default: out/persist.img, 16 MiB of zeros (formatted on first boot).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/out/persist.img}"
SIZE_MB="${2:-16}"

mkdir -p "$(dirname "$OUT")"
dd if=/dev/zero of="$OUT" bs=1048576 count="$SIZE_MB" status=none
echo "mkpersist: $OUT (${SIZE_MB} MiB zeros; NOSF @ LBA 2048 on first mount)"
