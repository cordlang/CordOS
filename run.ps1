$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath "build\cordos32.iso")) {
    & "$PSScriptRoot\build.ps1"
}

& "qemu-system-i386" -cdrom "build\cordos32.iso"
