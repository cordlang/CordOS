$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath "build\nuevoos.iso")) {
    & "$PSScriptRoot\build.ps1"
}

& "qemu-system-i386" -cdrom "build\nuevoos.iso"
