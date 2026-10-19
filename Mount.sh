
#!/usr/bin/env bash
# Mount.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

set -Eeuo pipefail

BaseDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ImgFile="${BaseDir}/LineOS/LineOS.img"
MountDir="/tmp/lineos_img_mount"

Action="${1:-mount}"
Partition="${2:-2}"

Error() {
    printf '[ERROR] %s\n' "$*" >&2
}

CheckImage() {
    if [[ ! -f "$ImgFile" ]]; then
        Error "Disk image not found."
        exit 1
    fi
}

CheckQEMU() {
    if command -v fuser >/dev/null 2>&1 &&
       fuser -s "$ImgFile" 2>/dev/null; then
        Error "Disk image is in use. Stop QEMU first."
        exit 1
    fi
}

FindLoop() {
    losetup -j "$ImgFile" |
        head -n 1 |
        cut -d: -f1
}

MountImage() {
    local LoopDev
    local NewLoop=false

    CheckImage
    CheckQEMU

    if [[ "$Partition" != "1" &&
          "$Partition" != "2" ]]; then
        Error "Partition must be 1 or 2."
        exit 1
    fi

    if mountpoint -q "$MountDir"; then
        Error "Already mounted: $MountDir"
        exit 1
    fi

    mkdir -p "$MountDir"

    LoopDev="$(FindLoop)"

    if [[ -z "$LoopDev" ]]; then
        LoopDev="$(sudo losetup -P -f --show "$ImgFile")"
        NewLoop=true
    fi

    sudo partprobe "$LoopDev" 2>/dev/null || true
    udevadm settle 2>/dev/null || true

    if ! sudo mount "${LoopDev}p${Partition}" "$MountDir"; then
        if [[ "$NewLoop" == true ]]; then
            sudo losetup -d "$LoopDev"
        fi
        exit 1
    fi

    printf '[MOUNT] %s\n' "${LoopDev}p${Partition}"
    printf '[PATH]  %s\n' "$MountDir"
}

UnmountImage() {
    local LoopDev

    LoopDev="$(FindLoop)"

    if mountpoint -q "$MountDir"; then
        sudo umount "$MountDir"
        printf '[UMOUNT] %s\n' "$MountDir"
    fi

    if [[ -n "$LoopDev" ]]; then
        if ! lsblk -nr -o MOUNTPOINT "$LoopDev" |
             grep -q '[^[:space:]]'; then
            sudo losetup -d "$LoopDev"
            printf '[DETACH] %s\n' "$LoopDev"
        fi
    fi
}

StatusImage() {
    local LoopDev

    LoopDev="$(FindLoop)"

    if mountpoint -q "$MountDir"; then
        findmnt "$MountDir"
    else
        printf '[STATUS] Not mounted\n'
    fi

    if [[ -n "$LoopDev" ]]; then
        printf '[LOOP] %s\n' "$LoopDev"
    fi
}

case "$Action" in
    mount)
        MountImage
        ;;
    umount|unmount)
        UnmountImage
        ;;
    status)
        StatusImage
        ;;
    *)
        Error "Usage: $0 [mount [1|2]|umount|status]"
        exit 1
        ;;
esac
