#!/usr/bin/env bash
# NuevoOS — assemble disk.img: MBR + stage2 + kernel ELF
# Usage: scripts/mkdisk.sh [kernel_elf] [out_img]
# Defaults: out/cordos.bin → out/disk.img
#
# Layout (docs/boot_protocol.md):
#   LBA 0        MBR (512 B)
#   LBA 1..64    stage2 (32 KiB)
#   LBA 65..     kernel ELF

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="${1:-$ROOT/out/cordos.bin}"
OUT="${2:-$ROOT/out/disk.img}"
MBR_BIN="${MBR_BIN:-$ROOT/out/mbr.bin}"
STAGE2_BIN="${STAGE2_BIN:-$ROOT/out/stage2.bin}"

STAGE2_SECTORS=64
KERNEL_LBA=$((1 + STAGE2_SECTORS))

die() { echo "mkdisk: $*" >&2; exit 1; }

[[ -f "$KERNEL" ]] || die "missing kernel: $KERNEL (make ARCH=x86_64 out/cordos.bin)"
[[ -f "$MBR_BIN" ]] || die "missing MBR: $MBR_BIN (make mbr)"
[[ -f "$STAGE2_BIN" ]] || die "missing stage2: $STAGE2_BIN (make mbr)"

KERNEL_SIZE=$(wc -c < "$KERNEL" | tr -d ' ')
KERNEL_SECTS=$(( (KERNEL_SIZE + 511) / 512 ))
(( KERNEL_SECTS >= 1 )) || die "kernel empty"
(( KERNEL_SECTS <= 512 )) || die "kernel too large ($KERNEL_SECTS sectors > 512)"

STAGE2_SIZE=$(wc -c < "$STAGE2_BIN" | tr -d ' ')
(( STAGE2_SIZE == STAGE2_SECTORS * 512 )) || \
  die "stage2 size $STAGE2_SIZE != $((STAGE2_SECTORS * 512))"

MBR_SIZE=$(wc -c < "$MBR_BIN" | tr -d ' ')
(( MBR_SIZE == 512 )) || die "MBR size $MBR_SIZE != 512"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

cp "$MBR_BIN" "$TMPDIR/mbr.bin"
cp "$STAGE2_BIN" "$TMPDIR/stage2.bin"

python3 - "$TMPDIR/stage2.bin" "$KERNEL_LBA" "$KERNEL_SECTS" <<'PY'
import struct, sys
path, lba, sects = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
data = bytearray(open(path, "rb").read())
magic = struct.pack("<I", 0x4F534E50)  # 'NOSP'
idx = data.rfind(magic)
if idx < 0:
    sys.exit("mkdisk: NOSP patch magic not found in stage2")
struct.pack_into("<II", data, idx + 4, lba, sects)
open(path, "wb").write(data)
print(f"mkdisk: stage2 kernel_lba={lba} kernel_sects={sects} @ {idx + 4}")
PY

TOTAL_SECTS=$((KERNEL_LBA + KERNEL_SECTS))
MIN_SECTS=$((2048 * 2))
if (( TOTAL_SECTS < MIN_SECTS )); then
  TOTAL_SECTS=$MIN_SECTS
fi

IMG="$TMPDIR/disk.img"
dd if=/dev/zero of="$IMG" bs=512 count="$TOTAL_SECTS" status=none
dd if="$TMPDIR/mbr.bin" of="$IMG" bs=512 count=1 conv=notrunc status=none
dd if="$TMPDIR/stage2.bin" of="$IMG" bs=512 seek=1 conv=notrunc status=none
dd if="$KERNEL" of="$IMG" bs=512 seek="$KERNEL_LBA" conv=notrunc status=none

mkdir -p "$(dirname "$OUT")"
cp "$IMG" "$OUT"
echo "mkdisk: wrote $OUT ($TOTAL_SECTS sectors, kernel=${KERNEL_SIZE}B @ LBA $KERNEL_LBA)"
