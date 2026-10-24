#!/usr/bin/env bash
# Scripts/Backend/Image.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

ImageDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ImageRoot="$(cd -- "$ImageDir/../.." && pwd)"

ImageCreate()
(
    local ImagePath="$ImageRoot/$DiskImage"
    local TemporaryImage=""
    local LoopDevice=""
    local Command

    ImageCleanup()
    {
        local Status=$?

        trap - EXIT

        if [[ -n "$LoopDevice" ]]; then
            if ! MountPrivilege losetup -d "$LoopDevice"; then
                printf 'Cannot detach %s; temporary image retained at %s\n' "$LoopDevice" "$TemporaryImage" >&2
                exit 1
            fi
        fi

        if [[ -n "$TemporaryImage" ]]; then
            if ! rm -f -- "$TemporaryImage"; then
                Status=1
            fi
        fi

        exit "$Status"
    }

    if ! [[ -n "${DiskImage:-}" && -n "${DiskSize:-}" && "${DiskEFISizeMiB:-}" =~ ^[1-9][0-9]*$ ]]; then
        printf 'Invalid disk image settings.\n' >&2
        return 1
    fi

    if [[ -e "$ImagePath" || -L "$ImagePath" ]]; then
        if [[ ! -f "$ImagePath" ]]; then
            printf 'Image path is not a regular file: %s\n' "$ImagePath" >&2
            return 1
        fi

        return 0
    fi

    for Command in truncate parted losetup mkfs.vfat mkfs.ext2 mktemp ln; do
        if ! command -v "$Command" >/dev/null 2>&1; then
            printf 'Missing command: %s\n' "$Command" >&2
            return 1
        fi
    done

    if ! mkdir -p "$(dirname "$ImagePath")"; then
        return 1
    fi

    trap ImageCleanup EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM

    if ! TemporaryImage=$(mktemp "$ImagePath.XXXXXXXX"); then
        return 1
    fi

    if ! truncate -s "$DiskSize" "$TemporaryImage"; then
        return 1
    fi

    if ! parted -s "$TemporaryImage" mklabel gpt mkpart ESP fat32 1MiB "$((DiskEFISizeMiB + 1))MiB" set 1 esp on mkpart LineOS ext2 "$((DiskEFISizeMiB + 1))MiB" 100%; then
        return 1
    fi

    if ! LoopDevice=$(MountPrivilege losetup --find --show --partscan "$TemporaryImage"); then
        return 1
    fi

    if ! MountPrivilege mkfs.vfat -F 32 -n LINEOS_EFI "${LoopDevice}p1"; then
        return 1
    fi

    if ! MountPrivilege mkfs.ext2 -F -L LineOS "${LoopDevice}p2"; then
        return 1
    fi

    if ! MountPrivilege losetup -d "$LoopDevice"; then
        return 1
    fi

    LoopDevice=""

    if ! ln -T -- "$TemporaryImage" "$ImagePath"; then
        printf 'Cannot publish new image without overwriting: %s\n' "$ImagePath" >&2
        return 1
    fi
)
