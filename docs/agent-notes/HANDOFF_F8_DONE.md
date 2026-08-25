# Handoff F8 COMPLETE

## Files
- `boot/mbr.s`, `boot/stage2.s`
- `scripts/mkdisk.sh`
- `docs/boot_protocol.md`

## Targets
```
make mbr disk
make run-bios   # experimental
make run        # GRUB (default, supported)
```

Needs NASM (`nasm` or `tools/nasm`).
