# Arranca dist\cordos.iso en VirtualBox (app de escritorio).
# Requisitos: ISO compilada (`make` en WSL) y VirtualBox instalado en Windows.

$ErrorActionPreference = "Stop"

$VmName = "CordOS"
$DistIsoPath = Join-Path $PSScriptRoot "dist\cordos.iso"
$IsoPath = $DistIsoPath
if (-not (Test-Path -LiteralPath $IsoPath)) {
    $IsoPath = Join-Path $PSScriptRoot "out\cordos.iso"
}
if (-not (Test-Path -LiteralPath $IsoPath)) {
    $IsoPath = Join-Path $PSScriptRoot "dist\nuevoos64.iso"
}
if (-not (Test-Path -LiteralPath $IsoPath)) {
    $IsoPath = Join-Path $PSScriptRoot "kbuild\cordos.iso"
}
if (-not (Test-Path -LiteralPath $IsoPath)) {
    $IsoPath = Join-Path $PSScriptRoot "build\cordos.iso"
}

function Find-VBoxManage {
    $candidates = @()
    if ($env:VBOX_MSI_INSTALL_PATH) {
        $candidates += (Join-Path $env:VBOX_MSI_INSTALL_PATH "VBoxManage.exe")
    }
    if ($env:VBOX_INSTALL_PATH) {
        $candidates += (Join-Path $env:VBOX_INSTALL_PATH "VBoxManage.exe")
    }
    $candidates += @(
        (Join-Path $env:ProgramFiles "Oracle\VirtualBox\VBoxManage.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Oracle\VirtualBox\VBoxManage.exe")
    )
    foreach ($path in $candidates) {
        if ($path -and (Test-Path -LiteralPath $path)) {
            return $path
        }
    }
    $cmd = Get-Command "VBoxManage.exe" -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    return $null
}

function Invoke-VBox {
    param([string[]]$VArgs)
    & $script:VBoxManage @VArgs
    if ($LASTEXITCODE -ne 0) {
        throw "VBoxManage failed: $($VArgs -join ' ')"
    }
}

function Invoke-VBoxSoft {
    param([string[]]$VArgs)
    # Best-effort: these calls are expected to fail on some hosts/VM states.
    # With the script-wide "Stop" preference, anything VBoxManage writes to
    # stderr would otherwise abort the whole run, so drop the preference for
    # the duration of the call and swallow both streams.
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $script:VBoxManage @VArgs 2>&1 | Out-Null
    } catch {
    } finally {
        $ErrorActionPreference = $prev
    }
}

if (-not (Test-Path -LiteralPath $IsoPath)) {
    Write-Host "No esta la ISO: $IsoPath"
    Write-Host "Compilala primero en WSL:"
    Write-Host "  wsl -d Ubuntu -- bash -lc 'cd /mnt/d/os && make ARCH=x86_64'"
    exit 1
}

$script:VBoxManage = Find-VBoxManage
if (-not $script:VBoxManage) {
    Write-Host "No encuentro VirtualBox (VBoxManage.exe)."
    Write-Host "Instala VirtualBox: https://www.virtualbox.org/wiki/Downloads"
    exit 1
}

$vmList = @(& $script:VBoxManage list vms)
$exists = $false
# VBoxManage is case-sensitive. The machine on this host is often
# registered as "cord", not "CordOS" — then startvm 404s and the user
# keeps booting the old VMSVGA VM with out\cordos.iso (the 1px line).
$vmAliases = @($VmName, "cordos", "cord")
foreach ($want in $vmAliases) {
    foreach ($line in $vmList) {
        if ($line -cmatch ('^"([^"]+)"') -and ($Matches[1] -ieq $want)) {
            $VmName = $Matches[1]
            $exists = $true
            break
        }
    }
    if ($exists) { break }
}

if (-not $exists) {
    Write-Host "Creando maquina virtual '$VmName' (64-bit, BIOS, raton PS/2)..."
    Invoke-VBox -VArgs @("createvm", "--name", $VmName, "--ostype", "Other_64", "--register")
    Invoke-VBox -VArgs @(
        "modifyvm", $VmName,
        "--memory", "512",
        "--cpus", "1",
        "--acpi", "on",
        "--ioapic", "on",
        "--pae", "on",
        "--hwvirtex", "on",
        "--nestedpaging", "on",
        "--boot1", "dvd",
        "--boot2", "none",
        "--boot3", "none",
        "--boot4", "none",
        "--vram", "128",
        "--mouse", "ps2",
        "--keyboard", "ps2",
        "--rtcuseutc", "on",
        "--nic1", "nat",
        "--nictype1", "82540EM",
        "--cableconnected1", "on"
    )
    Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--firmware", "bios")
    Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--graphicscontroller", "VBoxVGA")
    Invoke-VBox -VArgs @("storagectl", $VmName, "--name", "IDE", "--add", "ide", "--controller", "PIIX4")
    Write-Host "VM creada."
}

$running = & $script:VBoxManage list runningvms
$isRunning = $false
foreach ($line in $running) {
    if ($line -match ('^"' + [regex]::Escape($VmName) + '"')) {
        $isRunning = $true
        break
    }
}
if ($isRunning) {
    Write-Host "La VM '$VmName' ya esta en marcha. Cierra esa ventana o:"
    Write-Host "  `"$($script:VBoxManage)`" controlvm $VmName poweroff"
    exit 0
}

Add-Type -AssemblyName System.Windows.Forms | Out-Null
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$gw = [int]$bounds.Width
$gh = [int]$bounds.Height
if ($gw -lt 640) { $gw = 640 }
if ($gh -lt 400) { $gh = 400 }
# Guest = host native pixels (ultrawide, 1440p, 4K, 1366x768...).
if ($gw -gt 7680) { $gw = 7680 }
if ($gh -gt 4320) { $gh = 4320 }

Write-Host "Pantalla host: $($bounds.Width)x$($bounds.Height) -> invitado 1:1 ${gw}x${gh}"

$grubAuto = Join-Path $PSScriptRoot "kbuild\grub-auto.cfg"
$grubDir = Join-Path $PSScriptRoot "kbuild"
if (-not (Test-Path -LiteralPath $grubDir)) {
    New-Item -ItemType Directory -Path $grubDir | Out-Null
}
@(
    "set timeout=0"
    "set default=0"
    "insmod multiboot2"
    "insmod iso9660"
    "insmod vbe"
    "insmod vga"
    "insmod video_bochs"
    "insmod all_video"
    "insmod png"
    "set gfxpayload=${gw}x${gh}x32"
    "set color_normal=white/black"
    "background_image -m center /boot/splash.png"
    "menuentry `"CordOS`" {"
    "    insmod multiboot2"
    "    set gfxpayload=${gw}x${gh}x32"
    "    multiboot2 /boot/cordos.bin gfx=${gw}x${gh}"
    "    boot"
    "}"
) | Set-Content -LiteralPath $grubAuto -Encoding ascii

# The Windows and WSL clocks drift by a second or so, so a file just written
# from here looks like it comes from the future to make, which warns on stderr
# and aborts this script. Backdate it so the build stays quiet.
(Get-Item -LiteralPath $grubAuto).LastWriteTime = (Get-Date).AddMinutes(-5)

Write-Host "Reconstruyendo ISO con gfx=${gw}x${gh}..."
& wsl -d Ubuntu -- bash -lc "cd /mnt/d/os && make ARCH=x86_64 GRUBCFG=kbuild/grub-auto.cfg"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Aviso: no pude reconstruir la ISO; uso la existente."
} else {
    $builtIso = Join-Path $PSScriptRoot "out\cordos.iso"
    if (Test-Path -LiteralPath $builtIso) {
        if (-not (Test-Path -LiteralPath (Split-Path -Parent $DistIsoPath))) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $DistIsoPath) | Out-Null
        }
        Copy-Item -Force -LiteralPath $builtIso -Destination $DistIsoPath
        $IsoPath = $DistIsoPath
    }
}

Write-Host "Ajustando VirtualBox a ${gw}x${gh} 1:1 (VBoxVGA / Bochs VBE, sin zoom)..."
$vram = 128
if (($gw * $gh) -gt (1920 * 1080)) { $vram = 256 }
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--vram", "$vram")
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--memory", "512")
# VBoxSVGA is VMware SVGA; our kernel talks Bochs VBE DISPI. VBoxVGA honors
# virt-width / pitch. SVGA ignored that and packed rows so x=-1 wrapped to
# the other side of the wallpaper (1px L/R hairlines at the bottom).
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--graphicscontroller", "VBoxVGA")
Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/ScaleFactor", "1")
# Scaled Mode (View -> Scaled) is independent of ScaleFactor. It MUST be
# off: with it on, the 1920x1080 guest is bilinear-filtered down into the
# window, and that downscale invents content-dependent 1px hairlines (the
# "line" that seemed to depend on the onboarding language). The guest
# framebuffer itself is pixel-perfect; the artifact is 100% host scaling.
# Unset the key AND force it false so a prior Host+C toggle cannot linger.
Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/Scale")
Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/Scale", "false")
Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/MaxGuestResolution", "any")
Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/AutoresizeGuest", "false")
Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "CustomVideoMode1", "${gw}x${gh}x32")
Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/LastGuestSizeHint", "$gw,$gh")
$full = ($gw -eq [int]$bounds.Width) -and ($gh -eq [int]$bounds.Height)
if ($full) {
    Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/Fullscreen", "on")
} else {
    Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/Fullscreen", "off")
    # Width = guest; extra height is title/status only. gw+24 on a 1920
    # host does not fit and VBox scales (same hairline).
    Invoke-VBoxSoft -VArgs @("setextradata", $VmName, "GUI/LastNormalWindowPosition", "40,40,$gw,$($gh + 80)")
}

# Wi-Fi is the primary connection. VirtualBox has no 802.11 NIC, so the
# guest driver talks to a host helper over COM2 (named pipe) and uses
# NAT as the data plane. USB 2.0 is on for a real RTL8187 dongle.
#
# VirtualBox MUST own the pipe (server). If VBox is the client and the
# helper is not running, startvm dies with:
#   NamedPipe#0 ... \\.\pipe\cordos-wlan  (VERR_FILE_NOT_FOUND)
Write-Host "Red: NAT Intel 82540EM (ping) + Wi-Fi host"
Invoke-VBox -VArgs @(
    "modifyvm", $VmName,
    "--nic1", "nat",
    "--nictype1", "82540EM",
    "--cableconnected1", "on"
)
$nicInfo = & $script:VBoxManage showvminfo $VmName --machinereadable
$nic1 = ($nicInfo | Select-String -Pattern '^nic1=').Line
$nicType = ($nicInfo | Select-String -Pattern '^nictype1=').Line
Write-Host "  $nic1  $nicType"
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--usb", "on")
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--usbehci", "on")
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--usb-ohci", "on")
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--usb-ehci", "on")
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--uart2", "0x2F8", "3")

$wlanHelper = Join-Path $PSScriptRoot "tools\wlan_host.ps1"
$wlanPidFile = Join-Path $PSScriptRoot "out\wlan_host.pid"
if (Test-Path -LiteralPath $wlanPidFile) {
    $oldPid = Get-Content -LiteralPath $wlanPidFile -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($oldPid) {
        Stop-Process -Id $oldPid -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $wlanPidFile -Force -ErrorAction SilentlyContinue
}
# Release a leftover host-side pipe before VBox creates it as server.
Start-Sleep -Milliseconds 250
Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--uartmode2", "server", "\\.\pipe\cordos-wlan")

Write-Host "Montando ISO: $IsoPath"
Invoke-VBoxSoft -VArgs @("storageattach", $VmName, "--storagectl", "IDE", "--port", "0", "--device", "0", "--type", "dvddrive", "--medium", "emptydrive", "--forceunmount")
Invoke-VBoxSoft -VArgs @("closemedium", "dvd", $IsoPath, "--force")
Invoke-VBox -VArgs @(
    "storageattach", $VmName,
    "--storagectl", "IDE",
    "--port", "0",
    "--device", "0",
    "--type", "dvddrive",
    "--medium", $IsoPath,
    "--forceunmount"
)

Write-Host "Arrancando VirtualBox..."
try {
    Invoke-VBox -VArgs @("startvm", $VmName, "--type", "gui")
} catch {
    Write-Host "Arranque falló (pipe COM2). Reintento sin UART2..."
    Invoke-VBoxSoft -VArgs @("modifyvm", $VmName, "--uartmode2", "disconnected")
    Invoke-VBox -VArgs @("startvm", $VmName, "--type", "gui")
}

# Helper connects as client to the pipe VBox just created. Wi-Fi scan
# still works; if the helper is missing the guest just sees no SSIDs.
if (Test-Path -LiteralPath $wlanHelper) {
    $outDir = Join-Path $PSScriptRoot "out"
    if (-not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Path $outDir | Out-Null
    }
    $proc = Start-Process -FilePath "powershell.exe" -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $wlanHelper
    ) -WindowStyle Hidden -PassThru
    if ($proc) {
        $proc.Id | Set-Content -LiteralPath $wlanPidFile -Encoding ASCII
        Write-Host "Radio Wi-Fi del host: pipe cordos-wlan (pid $($proc.Id))"
    }
}
if ($full) {
    Start-Sleep -Milliseconds 800
    Invoke-VBoxSoft -VArgs @("controlvm", $VmName, "setvideomodehint", "$gw", "$gh", "32")
}
Write-Host "Listo. Login: admin / admin"
Write-Host "Invitado ${gw}x${gh} a 1:1 (sin escalar el contenido)."
Write-Host "Captura el raton con clic; Host (Right Ctrl) lo suelta."
Write-Host "Si ves una linea de 1px en los bordes: Host+F (pantalla completa) y View > Scaled Mode apagado."
