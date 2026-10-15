#!/usr/bin/env bash
# run.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

set -Eeuo pipefail

BaseDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RAM="4G"
QEMU="qemu-system-x86_64"
Accel="kvm"
CPU="host"
CPUCount="16"
CPUFlags="+tsc-deadline,+invtsc,+rdtscp,+arat,+fsgsbase,+pdpe1gb"

ImgFile="${BaseDir}/LineOS/LineOS.img"
ImgSize="8G"
EFISizeMiB=300
MountDir="/tmp/lineos_img_mount"

OVMF_CODE="${BaseDir}/uefi/OVMF_CODE.fd"
OVMF_VARS="${BaseDir}/uefi/OVMF_VARS.fd"

LogPath="${BaseDir}/logs/qemu.log"
DebugConLogPath="${BaseDir}/logs/debugcon.log"

BootFile="${BaseDir}/LineOS/EFI/BOOT/BOOTX64.EFI"
KernelFile="${BaseDir}/LineOS/KERNEL/LINEOS_KERNEL.ELF"

RTC="base=localtime,clock=host"
InputA="virtio-keyboard-pci"
InputB="virtio-tablet-pci"
Machine="q35"
VGA="none"
DisplayConfig="${LINEOS_DISPLAY:-gtk}"
GraphicsResInput="QHD"

Network="none"
DebugOption="int,guest_errors,cpu_reset,unimp"

CYAN=$'\033[36m'
YELLOW=$'\033[33m'
GREEN=$'\033[32m'
RED=$'\033[31m'
MAGENTA=$'\033[35m'
RESET=$'\033[0m'

if [[ ! -t 1 ]]; then
    CYAN=""
    YELLOW=""
    GREEN=""
    RED=""
    MAGENTA=""
    RESET=""
fi

Print() {
    local Color="$1"
    local Tag="$2"
    shift 2

    printf '%s[%s]%s %s\n' \
        "$Color" "$Tag" "$RESET" "$*"
}

Info() {
    Print "$CYAN" "*" "$*"
}

Success() {
    Print "$GREEN" "*" "$*"
}

Error() {
    Print "$RED" "-" "$*" >&2
}

RequireCommand() {
    if ! command -v "$1" >/dev/null 2>&1; then
        Error "Command not found: $1"
        return 1
    fi
}

LoopDev=""
Mounted=false

Cleanup() {
    local Status=$?
    trap - EXIT

    if [[ "$Mounted" == true ]]; then
        sudo umount "$MountDir" || true
        Mounted=false
    fi

    if [[ -n "$LoopDev" ]]; then
        sudo losetup -d "$LoopDev" || true
        LoopDev=""
    fi

    if (( Status != 0 )); then
        Error "Builder exited with status $Status."
    fi

    exit "$Status"
}

trap Cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

SetupGraphics() {
    case "$GraphicsResInput" in
        HD)
            GraphicsWidth=1280
            GraphicsHeight=720
            ;;
        FHD)
            GraphicsWidth=1920
            GraphicsHeight=1080
            ;;
        FHD+)
            GraphicsWidth=2240
            GraphicsHeight=1260
            ;;
        QHD)
            GraphicsWidth=2560
            GraphicsHeight=1440
            ;;
        UHD)
            GraphicsWidth=3840
            GraphicsHeight=2160
            ;;
        CUSTOM)
            GraphicsWidth=2560
            GraphicsHeight=1400
            ;;
        *)
            Error "Unknown resolution: $GraphicsResInput"
            return 1
            ;;
    esac

    GraphicsResolution="${GraphicsWidth}x${GraphicsHeight}"
    VideoDevice="virtio-gpu-pci,xres=${GraphicsWidth},yres=${GraphicsHeight},hostmem=1G"
}

AttachImage() {
    LoopDev="$(sudo losetup -P -f --show "$ImgFile")"

    if [[ -z "$LoopDev" ]]; then
        Error "Failed to attach disk image."
        return 1
    fi

    sudo partprobe "$LoopDev" 2>/dev/null || true
    udevadm settle 2>/dev/null || true

    if [[ ! -b "${LoopDev}p1" || ! -b "${LoopDev}p2" ]]; then
        Error "Image partitions not found."
        return 1
    fi
}

DetachImage() {
    if [[ "$Mounted" == true ]]; then
        sudo umount "$MountDir"
        Mounted=false
        Print "$CYAN" "UMNT" "EFI partition"
    fi

    if [[ -n "$LoopDev" ]]; then
        sudo losetup -d "$LoopDev"
        LoopDev=""
    fi
}

CreateImg() {
    if [[ -f "$ImgFile" ]]; then
        Print "$GREEN" "IMG" "Existing image found."
        return 0
    fi

    RequireCommand qemu-img
    RequireCommand parted
    RequireCommand mkfs.vfat
    RequireCommand mkfs.ext2
    RequireCommand losetup

    mkdir -p "$(dirname "$ImgFile")"

    Print "$CYAN" "IMG" "Creating ${ImgSize} raw image..."
    qemu-img create -f raw "$ImgFile" "$ImgSize" >/dev/null

    Print "$YELLOW" "GPT" "Creating partition table..."
    sudo parted -s "$ImgFile" \
        mklabel gpt \
        mkpart EFI fat32 1MiB "$((EFISizeMiB + 1))MiB" \
        set 1 esp on \
        mkpart LineOS "$((EFISizeMiB + 1))MiB" 100%

    AttachImage

    Print "$YELLOW" "FS" "Formatting EFI as FAT32..."
    sudo mkfs.vfat -F 32 -n LINEOS_EFI "${LoopDev}p1" >/dev/null

    Print "$YELLOW" "FS" "Formatting root as EXT2..."
    sudo mkfs.ext2 -F -L LINEOS_ROOT "${LoopDev}p2" >/dev/null

    DetachImage
    Success "Disk image created."
}

CopyImg() {
    Info "Image"

    CreateImg

    RequireCommand mountpoint
    RequireCommand mount

    if mountpoint -q "$MountDir"; then
        Error "Mount directory is already in use: $MountDir"
        return 1
    fi

    mkdir -p "$MountDir"
    AttachImage

    Print "$CYAN" "MNT" "Mounting EFI partition..."
    sudo mount "${LoopDev}p1" "$MountDir"
    Mounted=true

    sudo mkdir -p \
        "${MountDir}/EFI/BOOT" \
        "${MountDir}/KERNEL"

    Print "$CYAN" "CP" "BOOTX64.EFI"
    sudo cp "$BootFile" \
        "${MountDir}/EFI/BOOT/BOOTX64.EFI"

    Print "$CYAN" "CP" "LINEOS_KERNEL.ELF"
    sudo cp "$KernelFile" \
        "${MountDir}/KERNEL/LINEOS_KERNEL.ELF"

    sync
    DetachImage

    Success "Image ready."
}

ResolveOVMF() {
    if [[ ! -f "$OVMF_CODE" ]]; then
        for Candidate in \
            /usr/share/OVMF/OVMF_CODE_4M.fd \
            /usr/share/OVMF/OVMF_CODE.fd
        do
            if [[ -f "$Candidate" ]]; then
                OVMF_CODE="$Candidate"
                break
            fi
        done
    fi

    if [[ ! -f "$OVMF_VARS" ]]; then
        local Template=""

        for Candidate in \
            /usr/share/OVMF/OVMF_VARS_4M.fd \
            /usr/share/OVMF/OVMF_VARS.fd
        do
            if [[ -f "$Candidate" ]]; then
                Template="$Candidate"
                break
            fi
        done

        if [[ -z "$Template" ]]; then
            Error "OVMF variables template not found."
            return 1
        fi

        mkdir -p "$(dirname "$OVMF_VARS")"
        cp "$Template" "$OVMF_VARS"
    fi

    if [[ ! -f "$OVMF_CODE" ]]; then
        Error "OVMF firmware not found."
        return 1
    fi
}

StartQEMU() {
    Info "QEMU"

    RequireCommand "$QEMU"
    ResolveOVMF

    mkdir -p "${BaseDir}/logs"

    Print "$CYAN" "VM" "Machine: $Machine"
    Print "$CYAN" "VM" "CPU: $CPU ($CPUCount vCPUs)"
    Print "$CYAN" "VM" "Memory: $RAM"
    Print "$CYAN" "VM" "Display: $DisplayConfig ($GraphicsResolution)"

    local QEMU_ARGS=(
        -rtc "$RTC"
        -accel "$Accel"
        -cpu "$CPU,$CPUFlags"
        -smp "$CPUCount"
        -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE"
        -drive "if=pflash,format=raw,file=$OVMF_VARS"
        -net "$Network"
        -M "$Machine"
        -device "$InputA"
        -device "$InputB"
        -vga "$VGA"
        -device "$VideoDevice"
        -drive "file=$ImgFile,format=raw,id=lineos_disk,if=none"
        -device "virtio-blk-pci,drive=lineos_disk,bootindex=0"
        -fw_cfg "name=opt/org.tianocore/GraphicsResolution,string=$GraphicsResolution"
        -no-reboot
        -no-shutdown
        -d "$DebugOption"
        -D "$LogPath"
        -debugcon "file:$DebugConLogPath"
        -global "isa-debugcon.iobase=0xe9"
        -m "$RAM"
        -display "$DisplayConfig"
    )

    rm -f "$LogPath" "$DebugConLogPath"

    Print "$MAGENTA" "VM" "Starting QEMU..."

    local Status=0
    "$QEMU" "${QEMU_ARGS[@]}" || Status=$?

    if [[ -f "$LogPath" ]] &&
       grep -Eiq 'triple fault|cpu_reset' "$LogPath"; then
        Error "CPU reset or triple fault detected."
        return 1
    fi

    if (( Status != 0 )); then
        Error "QEMU exited with status $Status."
        return "$Status"
    fi

    Success "QEMU stopped."
}

Main() {
    cd "$BaseDir"

    SetupGraphics

    printf '%sLineOS Builder v2.8.0%s\n' "$YELLOW" "$RESET"
    printf '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n'

    Info "Build"

    RequireCommand make
    RequireCommand nproc

    make -j"$(nproc)"

    Success "Build complete."
    printf '\n'

    CopyImg
    printf '\n'

    StartQEMU
    printf '\n'

    Print "$MAGENTA" "*" "Exit."
}

Main "$@"
