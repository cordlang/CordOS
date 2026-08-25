# Handoff F7 COMPLETE

## Files
- `src/shell.c` + `src/include/shell.h`
- Wired into `kmain64` as `shell_run()` (replaces echo loop)
- `build/shell.o` may already be in Makefile

## VFS contract for F6
Shell uses `__has_include("vfs.h")` or weak symbols:
- `vfs_open` / `vfs_read` / `vfs_close`
- `vfs_ls` (or equivalent listing)

Without FS: prints `(vfs pending)`.

## INT
Confirm Makefile has `build/shell.o`; call order: ... → phaseN_inits → `shell_run()` last (blocking).
