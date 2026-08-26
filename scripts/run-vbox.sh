#!/usr/bin/env bash
# Arranca dist/cordos.iso en VirtualBox (Linux host).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ISO="$ROOT/dist/cordos.iso"
if [[ ! -f "$ISO" ]]; then
    ISO="$ROOT/out/cordos.iso"
fi
VM_NAME="CordOS"

if [[ ! -f "$ISO" ]]; then
    echo "No esta la ISO: $ISO"
    echo "Compila antes: make ARCH=x86_64"
    exit 1
fi

if ! command -v VBoxManage >/dev/null 2>&1; then
    echo "VBoxManage no esta en PATH."
    echo "En Windows usa: ./run-vbox.ps1"
    exit 1
fi

if ! VBoxManage showvminfo "$VM_NAME" >/dev/null 2>&1; then
    echo "Creando maquina virtual '$VM_NAME' (64-bit, BIOS, raton PS/2)..."
    VBoxManage createvm --name "$VM_NAME" --ostype Other_64 --register
    VBoxManage modifyvm "$VM_NAME" \
        --memory 512 \
        --cpus 1 \
        --acpi on \
        --ioapic on \
        --pae on \
        --hwvirtex on \
        --nestedpaging on \
        --boot1 dvd \
        --boot2 none \
        --boot3 none \
        --boot4 none \
        --vram 128 \
        --mouse ps2 \
        --keyboard ps2 \
        --rtcuseutc on \
        --nic1 nat \
        --nictype1 82540EM \
        --cableconnected1 on
    VBoxManage modifyvm "$VM_NAME" --firmware bios >/dev/null 2>&1 || true
    VBoxManage modifyvm "$VM_NAME" --graphicscontroller VBoxVGA >/dev/null 2>&1 || true
    VBoxManage storagectl "$VM_NAME" --name IDE --add ide --controller PIIX4
    VBoxManage storagectl "$VM_NAME" --name SATA --add sata --controller IntelAhci \
        --portcount 2 --bootable off
fi

if VBoxManage list runningvms | grep -q "\"$VM_NAME\""; then
    echo "La VM '$VM_NAME' ya esta en marcha."
    exit 0
fi

HOST_W=1920
HOST_H=1080
if command -v xrandr >/dev/null 2>&1; then
    read -r HOST_W HOST_H < <(xrandr | awk '/\*/{gsub(/x/," ",$1); print $1,$2; exit}')
fi
GW=$HOST_W
GH=$HOST_H
[ "$GW" -gt 7680 ] && GW=7680
[ "$GH" -gt 4320 ] && GH=4320
[ "$GW" -lt 640 ] && GW=640
[ "$GH" -lt 400 ] && GH=400
echo "Pantalla host: ${HOST_W}x${HOST_H} -> invitado 1:1 ${GW}x${GH}"

mkdir -p "$ROOT/build"
cat > "$ROOT/build/grub-auto.cfg" <<EOF
set timeout=0
set default=0
insmod multiboot2
insmod iso9660
insmod vbe
insmod vga
insmod video_bochs
insmod png
set gfxpayload=${GW}x${GH}x32
set color_normal=white/black
background_image -m center /boot/splash.png
menuentry "CordOS" {
    insmod multiboot2
    set gfxpayload=${GW}x${GH}x32
    multiboot2 /boot/cordos.bin gfx=${GW}x${GH}
    boot
}
EOF
make -C "$ROOT" ARCH=x86_64 GRUBCFG=build/grub-auto.cfg || echo "Aviso: ISO no reconstruida"
if [[ -f "$ROOT/dist/cordos.iso" ]]; then
    ISO="$ROOT/dist/cordos.iso"
fi

echo "Ajustando VirtualBox a ${GW}x${GH} 1:1 (sin zoom)..."
VBoxManage modifyvm "$VM_NAME" --nic1 nat --nictype1 82540EM --cableconnected1 on
VBoxManage modifyvm "$VM_NAME" --vram 128 >/dev/null 2>&1 || true
VBoxManage modifyvm "$VM_NAME" --memory 512 >/dev/null 2>&1 || true
    VBoxManage modifyvm "$VM_NAME" --graphicscontroller VBoxVGA >/dev/null 2>&1 || true
VBoxManage setextradata "$VM_NAME" GUI/ScaleFactor 1 >/dev/null 2>&1 || true
VBoxManage setextradata "$VM_NAME" GUI/MaxGuestResolution any >/dev/null 2>&1 || true
VBoxManage setextradata "$VM_NAME" GUI/AutoresizeGuest false >/dev/null 2>&1 || true
VBoxManage setextradata "$VM_NAME" CustomVideoMode1 "${GW}x${GH}x32" >/dev/null 2>&1 || true
VBoxManage setextradata "$VM_NAME" GUI/LastGuestSizeHint "${GW},${GH}" >/dev/null 2>&1 || true
if [ "$GW" = "$HOST_W" ] && [ "$GH" = "$HOST_H" ]; then
    VBoxManage setextradata "$VM_NAME" GUI/Fullscreen on >/dev/null 2>&1 || true
else
    VBoxManage setextradata "$VM_NAME" GUI/Fullscreen off >/dev/null 2>&1 || true
    VBoxManage setextradata "$VM_NAME" GUI/LastNormalWindowPosition "80,40,${GW},${GH}" >/dev/null 2>&1 || true
fi

echo "Montando ISO: $ISO"
VBoxManage storageattach "$VM_NAME" --storagectl IDE --port 0 --device 0 \
    --type dvddrive --medium emptydrive --forceunmount >/dev/null 2>&1 || true
VBoxManage closemedium dvd "$ISO" --force >/dev/null 2>&1 || true
VBoxManage storageattach "$VM_NAME" --storagectl IDE --port 0 --device 0 \
    --type dvddrive --medium "$ISO" --forceunmount

PERSIST_IMG="$ROOT/out/persist.img"
PERSIST_VDI="$ROOT/out/persist.vdi"
INFO="$(VBoxManage showvminfo "$VM_NAME" --machinereadable)"
SATA_NAME=""
i=0
while [ "$i" -le 7 ]; do
    name="$(printf '%s\n' "$INFO" | sed -n "s/^storagecontrollername${i}=\"\\(.*\\)\"$/\\1/p")"
    typ="$(printf '%s\n' "$INFO" | sed -n "s/^storagecontrollertype${i}=\"\\(.*\\)\"$/\\1/p")"
    if [ "$typ" = "IntelAhci" ]; then
        SATA_NAME="$name"
        break
    fi
    i=$((i + 1))
done

if [[ -z "$SATA_NAME" ]]; then
    echo "Anadiendo SATA AHCI (disco interno del PC)..."
    VBoxManage storagectl "$VM_NAME" --name SATA --add sata --controller IntelAhci \
        --portcount 2 --bootable off
    SATA_NAME="SATA"
    INFO="$(VBoxManage showvminfo "$VM_NAME" --machinereadable)"
fi

slot_val() {
    local key="$1"
    printf '%s\n' "$INFO" | sed -n "s/^\"${key}\"=\"\\(.*\\)\"$/\\1/p"
}

SATA00="$(slot_val "${SATA_NAME}-0-0")"
IDE10="$(slot_val "IDE-1-0")"

if [[ -n "$SATA00" && "$SATA00" != "none" ]]; then
    echo "Disco del sistema (SATA 0 / AHCI): $SATA00"
    if [[ -n "$IDE10" && "$IDE10" != "none" ]]; then
        echo "Quitando HDD extra de IDE 1:0 (el sistema vive en SATA)"
        VBoxManage storageattach "$VM_NAME" --storagectl IDE --port 1 --device 0 \
            --medium none >/dev/null 2>&1 || true
    fi
else
    mkdir -p "$ROOT/out"
    MEDIUM=""
    if [[ -n "$IDE10" && "$IDE10" != "none" ]]; then
        echo "Moviendo disco de IDE 1:0 a SATA (como un SSD interno)..."
        VBoxManage storageattach "$VM_NAME" --storagectl IDE --port 1 --device 0 \
            --medium none
        MEDIUM="$IDE10"
    else
        if [[ ! -f "$PERSIST_VDI" ]]; then
            if [[ ! -f "$PERSIST_IMG" ]]; then
                bash "$ROOT/scripts/mkpersist.sh" "$PERSIST_IMG"
            fi
            VBoxManage convertfromraw "$PERSIST_IMG" "$PERSIST_VDI" --format VDI
        fi
        MEDIUM="$PERSIST_VDI"
    fi
    echo "Montando disco del sistema: $MEDIUM (SATA 0 / AHCI)"
    VBoxManage storageattach "$VM_NAME" --storagectl "$SATA_NAME" --port 0 --device 0 \
        --type hdd --medium "$MEDIUM"
fi

echo "Arrancando VirtualBox..."
VBoxManage startvm "$VM_NAME" --type gui
echo "Login: admin / admin  |  invitado ${GW}x${GH} 1:1"
