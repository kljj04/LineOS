#!/bin/bash

BaseDir="$(pwd)"
RAM="4G"
LogFile="qemu.log"
QEMU="qemu-system-x86_64"
Accel="kvm"
CPU="host"
CPUFlags="+tsc-deadline,+invtsc"
VHD="${BaseDir}/LineOS/LineOS.vhdx"
MountDir="/tmp/lineos_vhd_mount"
OVMF_CODE="${BaseDir}/uefi/OVMF_CODE.fd"
OVMF_VARS="${BaseDir}/uefi/OVMF_VARS.fd"
LogPath="${BaseDir}/logs/${LogFile}"
BootFile="${BaseDir}/LineOS/EFI/BOOT/BOOTX64.EFI"
KernelFile="${BaseDir}/LineOS/KERNEL/LINEOS_KERNEL.ELF"
RTC="base=localtime,clock=host"
Machine="pc"
VGA="std"
GraphicsResolution="2560x1440"
Network="none"
DebugOption="guest_errors,cpu_reset"

CYAN="\033[36m"
YELLOW="\033[33m"
GREEN="\033[32m"
RED="\033[31m"
MAGENTA="\033[35m"
RESET="\033[0m"

DismountVHD() {
    local Silent=$1
    if mountpoint -q "$MountDir" 2>/dev/null; then
        sudo umount -l "$MountDir" 2>/dev/null
        if [ "$Silent" != "true" ]; then
            echo -e "${CYAN}    [*] VHD unmounted.${RESET}"
        fi
    fi
    sudo qemu-nbd --disconnect /dev/nbd0 >/dev/null 2>&1
    sudo rm -rf "$MountDir"
}

CopyVHD() {
    if [ ! -f "$VHD" ]; then
        echo -e "${RED}    [-] VHD file not found: $VHD${RESET}"
        return 1
    fi

    sudo rm -rf "$MountDir"
    mkdir -p "$MountDir"

    sudo modprobe nbd max_part=8 2>/dev/null
    sudo qemu-nbd --connect=/dev/nbd0 "$VHD" 2>/dev/null
    sleep 0.5

    if ! sudo mount /dev/nbd0p1 "$MountDir" 2>/dev/null; then
        sudo mount /dev/nbd0 "$MountDir" 2>/dev/null
    fi

    echo -e "${CYAN}    [*] VHD mounted.${RESET}"

    sudo mkdir -p "${MountDir}/EFI/BOOT"
    sudo mkdir -p "${MountDir}/KERNEL"

    sudo rm -f "${MountDir}/EFI/BOOT/BOOTX64.EFI" 2>/dev/null
    sudo rm -f "${MountDir}/KERNEL/LINEOS_KERNEL.ELF" 2>/dev/null

    sudo cp -f "$BootFile" "${MountDir}/EFI/BOOT/BOOTX64.EFI"
    sudo cp -f "$KernelFile" "${MountDir}/KERNEL/LINEOS_KERNEL.ELF"

    echo -e "${CYAN}    [*] Copy complete.${RESET}"

    sudo umount -l "$MountDir" 2>/dev/null
    sudo qemu-nbd --disconnect /dev/nbd0 >/dev/null 2>&1
    echo -e "${CYAN}    [*] VHD unmounted.${RESET}"
    return 0
}

StartQEMU() {
    mkdir -p "${BaseDir}/LineOS"
    mkdir -p "${BaseDir}/logs"

    if ! command -v $QEMU &> /dev/null; then
        echo -e "${RED}    [-] QEMU not found.${RESET}"
        return 1
    fi

    echo -e "${CYAN}    [*] QEMU start...${RESET}"

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
        -fw_cfg "name=opt/org.tianocore/GraphicsResolution,string=$GraphicsResolution" \
        -no-reboot \
        -d $DebugOption \
        -D "$LogPath" \
        -m $RAM \
        -drive "file=$VHD,format=vhdx" \
        #2> /dev/null

    echo -e "${GREEN}    [*] Done.${RESET}"
}

DismountVHD true
echo -e "${YELLOW}LineOS Builder v2.4.0 (Linux Native)${RESET}"

echo -e "${CYAN}[*] make:${RESET}"
make
if [ $? -ne 0 ]; then
    echo -e "${RED}    [-] make failed.${RESET}"
    exit 1
fi
echo -e "${CYAN}---End of make---${RESET}"

echo -e "${CYAN}[*] vhd:${RESET}"
if ! CopyVHD; then
    DismountVHD true
    exit 1
fi
echo -e "${CYAN}---End of vhd---${RESET}"

echo -e "${CYAN}[*] run:${RESET}"
StartQEMU

if [ -f "$LogPath" ]; then
    if grep -E -q "cpu_reset|Triple fault|triple fault|Triple Fault" "$LogPath"; then
        echo -e "${RED}    [!!] triple fault detected in log!${RESET}"
        exit 1
    fi
fi

echo -e "${CYAN}---End of run---${RESET}"
echo -e "${MAGENTA}[*] Exit.${RESET}"
DismountVHD true