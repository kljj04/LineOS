#!/bin/bash
# run.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CYAN="\033[36m"
YELLOW="\033[33m"
GREEN="\033[32m"
RED="\033[31m"
MAGENTA="\033[35m"
RESET="\033[0m"

BaseDir="$(pwd)"
RAM="4G"
LogFile="qemu.log"
DebugConFile="debugcon.log"
QEMU="qemu-system-x86_64"
Accel="kvm"
CPU="host"
CPUFlags="+tsc-deadline,+invtsc,+fsgsbase,+pdpe1gb"
ImgFile="${BaseDir}/LineOS/LineOS.img"
ImgSize="8G"
EFISizeMiB="300"
MountDir="/tmp/lineos_img_mount"
OVMF_CODE="${BaseDir}/uefi/OVMF_CODE.fd"
OVMF_VARS="${BaseDir}/uefi/OVMF_VARS.fd"
LogPath="${BaseDir}/logs/${LogFile}"
DebugConPath="${BaseDir}/logs/${DebugConFile}"
BootFile="${BaseDir}/LineOS/EFI/BOOT/BOOTX64.EFI"
KernelFile="${BaseDir}/LineOS/KERNEL/LINEOS_KERNEL.ELF"
RTC="base=localtime,clock=host"
Machine="q35"
VGA="none"
DisplayConfig="gtk"
GraphicsResInput="QHD"

if [ "$GraphicsResInput" = "HD" ]; then
  GraphicsWidth="1280"
  GraphicsHeight="720"
elif [ "$GraphicsResInput" = "FHD" ]; then
  GraphicsWidth="1920"
  GraphicsHeight="1080"
elif [ "$GraphicsResInput" = "FHD+" ]; then
  GraphicsWidth="2240"
  GraphicsHeight="1260"
elif [ "$GraphicsResInput" = "QHD" ]; then
  GraphicsWidth="2560"
  GraphicsHeight="1440"
elif [ "$GraphicsResInput" = "UHD" ]; then
  GraphicsWidth="3840"
  GraphicsHeight="2160"
else
  echo -e "${RED}[-] Unknown resolution.${RESET}"
  exit 1
fi

GraphicsResolution="${GraphicsWidth}x${GraphicsHeight}"
VideoDevice="virtio-gpu-pci,xres=${GraphicsWidth},yres=${GraphicsHeight}"
BlkDevice="virtio-blk-pci,drive=lineos_disk,bootindex=0"

Network="none"
DebugOption="guest_errors,cpu_reset"

RequireCommand() {
    local Command=$1

    if ! command -v "$Command" &>/dev/null; then
        echo -e "${RED}    [-] $Command not found.${RESET}"
        return 1
    fi

    return 0
}

DismountImg() {
    local Silent=$1

    if mountpoint -q "$MountDir" 2>/dev/null; then
        sudo umount -l "$MountDir" 2>/dev/null
        if [ "$Silent" != "true" ]; then
            echo -e "${CYAN}    [*] Image unmounted.${RESET}"
        fi
    fi

    if [ -f "$ImgFile" ]; then
        local LoopDevs
        LoopDevs=$(losetup -j "$ImgFile" | cut -d: -f1)
        for dev in $LoopDevs; do
            sudo losetup -d "$dev" 2>/dev/null
        done
    fi

    sudo rm -rf "$MountDir"
}

CreateImg() {
    local LoopDev

    if [ -f "$ImgFile" ]; then
        return 0
    fi

    RequireCommand qemu-img || return 1
    RequireCommand parted || return 1
    RequireCommand mkfs.vfat || return 1
    RequireCommand mkfs.ext3 || return 1
    RequireCommand losetup || return 1

    mkdir -p "$(dirname "$ImgFile")"

    echo -e "${CYAN}    [*] Create raw image.${RESET}"
    qemu-img create -f raw "$ImgFile" "$ImgSize" >/dev/null 2>&1 || return 1

    echo -e "${CYAN}    [*] Partition image.${RESET}"
    sudo parted -s "$ImgFile" \
        mklabel gpt \
        mkpart EFI fat32 1MiB "$((EFISizeMiB + 1))MiB" \
        set 1 esp on \
        mkpart LineOS "$((EFISizeMiB + 1))MiB" 100% >/dev/null 2>&1 || return 1

    LoopDev=$(sudo losetup -P -f --show "$ImgFile" 2>/dev/null)
    if [ -z "$LoopDev" ]; then
        echo -e "${RED}    [-] Failed to setup loop device.${RESET}"
        return 1
    fi

    sleep 0.2

    if [ ! -b "${LoopDev}p1" ]; then
        echo -e "${RED}    [-] EFI partition not found on $LoopDev.${RESET}"
        sudo losetup -d "$LoopDev" 2>/dev/null
        return 1
    fi

    if [ ! -b "${LoopDev}p2" ]; then
        echo -e "${RED}    [-] EXT3 partition not found on $LoopDev.${RESET}"
        sudo losetup -d "$LoopDev" 2>/dev/null
        return 1
    fi

    sudo mkfs.vfat -F 32 -n LINEOS_EFI "${LoopDev}p1" >/dev/null 2>&1
    sudo mkfs.ext3 -F -L LINEOS_ROOT "${LoopDev}p2" >/dev/null 2>&1
    sudo losetup -d "$LoopDev" 2>/dev/null

    echo -e "${CYAN}    [*] Image ready.${RESET}"
    return 0
}

CopyImg() {
    CreateImg || return 1

    sudo rm -rf "$MountDir"
    mkdir -p "$MountDir"

    local LoopDev
    LoopDev=$(sudo losetup -P -f --show "$ImgFile" 2>/dev/null)

    if [ -z "$LoopDev" ]; then
        echo -e "${RED}    [-] Failed to setup loop device.${RESET}"
        return 1
    fi

    sleep 0.2

    if [ -b "${LoopDev}p1" ]; then
        sudo mount "${LoopDev}p1" "$MountDir" 2>/dev/null
    else
        sudo mount "$LoopDev" "$MountDir" 2>/dev/null
    fi

    if ! mountpoint -q "$MountDir"; then
        echo -e "${RED}    [-] Mount failed on $LoopDev${RESET}"
        sudo losetup -d "$LoopDev" 2>/dev/null
        return 1
    fi

    echo -e "${CYAN}    [*] Image mounted.${RESET}"

    sudo mkdir -p "${MountDir}/EFI/BOOT"
    sudo mkdir -p "${MountDir}/KERNEL"

    sudo rm -f "${MountDir}/EFI/BOOT/BOOTX64.EFI" 2>/dev/null
    sudo rm -f "${MountDir}/KERNEL/LINEOS_KERNEL.ELF" 2>/dev/null

    sudo cp -f "$BootFile" "${MountDir}/EFI/BOOT/BOOTX64.EFI"
    sudo cp -f "$KernelFile" "${MountDir}/KERNEL/LINEOS_KERNEL.ELF"

    sync

    echo -e "${CYAN}    [*] Copy complete.${RESET}"

    sudo umount -l "$MountDir" 2>/dev/null
    sudo losetup -d "$LoopDev" 2>/dev/null
    echo -e "${CYAN}    [*] Image unmounted.${RESET}"
    return 0
}

StartQEMU() {
    mkdir -p "${BaseDir}/LineOS"
    mkdir -p "${BaseDir}/logs"

    RequireCommand "$QEMU" || return 1

    echo -e "${CYAN}    [*] QEMU start...${RESET}"
    rm -f "$DebugConPath"

    if [ ! -f "$OVMF_VARS" ]; then
        OVMF_VARS="/usr/share/OVMF/OVMF_VARS.fd"
    fi

    $QEMU \
        -rtc $RTC \
        -accel $Accel \
        -cpu $CPU,$CPUFlags \
        -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" \
        -drive "if=pflash,format=raw,file=$OVMF_VARS" \
        -net $Network \
        -M $Machine \
        -vga $VGA \
        -device "$VideoDevice" \
        -drive "file=$ImgFile,format=raw,id=lineos_disk,if=none" \
        -device "$BlkDevice" \
        -fw_cfg "name=opt/org.tianocore/GraphicsResolution,string=$GraphicsResolution" \
        -no-reboot \
        -d $DebugOption \
        -D "$LogPath" \
        -debugcon "file:$DebugConPath" \
        -global isa-debugcon.iobase=0xe9 \
        -m $RAM \
        -display $DisplayConfig \
        2> /dev/null

    echo -e "${GREEN}    [*] Done.${RESET}"
}

DismountImg true
echo -e "${YELLOW}LineOS Builder v2.7.0 (Linux Native Raw)${RESET}"

echo -e "${CYAN}[*] make:${RESET}"
make
if [ $? -ne 0 ]; then
    echo -e "${RED}    [-] make failed.${RESET}"
    exit 1
fi
echo -e "${CYAN}━━━End of make━━━${RESET}"

echo -e "${CYAN}[*] img:${RESET}"
if ! CopyImg; then
    DismountImg true
    exit 1
fi
echo -e "${CYAN}━━━End of img━━━${RESET}"

echo -e "${CYAN}[*] run:${RESET}"
StartQEMU

if [ -f "$LogPath" ]; then
    if grep -E -q "cpu_reset|Triple fault|triple fault|Triple Fault" "$LogPath"; then
        echo -e "${RED}    [!!] Triple Fault.${RESET}"
        exit 1
    fi
fi

echo -e "${CYAN}━━━End of run━━━${RESET}"
echo -e "${MAGENTA}[*] Exit.${RESET}"
DismountImg true
