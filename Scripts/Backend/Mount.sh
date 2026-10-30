#!/usr/bin/env bash
# Scripts/Backend/Mount.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

MountDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
MountRoot="$(cd -- "$MountDir/../.." && pwd)"

MountAuthenticate()
{
    if ! sudo -v; then
        printf 'Disk operations require sudo authentication.\n' >&2
        return 1
    fi
}

MountPrivilege()
{
    sudo -n -- "$@"
}

MountReleaseManaged()
{
    local ImagePath="$MountRoot/$DiskImage"
    local MountBase="$MountRoot/LineOS/Mount"
    local Existing LoopDevice Partition MountPoint IDs ManagedIDs
    local Found

    if ! Existing=$(losetup --associated "$ImagePath" --noheadings --output NAME); then
        return 1
    fi

    if [[ -z "$Existing" ]]; then
        return 0
    fi

    while IFS= read -r LoopDevice; do
        Found=0

        if findmnt --noheadings --source "$LoopDevice" >/dev/null; then
            printf 'Image is mounted outside the managed directories: %s\n' "$LoopDevice" >&2
            return 1
        fi

        for Partition in 1 2; do
            MountPoint="$MountBase/EFI"

            if (( Partition == 2 )); then
                MountPoint="$MountBase/Root"
            fi

            if IDs=$(findmnt --raw --noheadings --source "${LoopDevice}p${Partition}" --output ID); then
                if ! ManagedIDs=$(findmnt --raw --noheadings --source "${LoopDevice}p${Partition}" --mountpoint "$MountPoint" --output ID); then
                    printf 'Image partition is mounted elsewhere: %sp%s\n' "$LoopDevice" "$Partition" >&2
                    return 1
                fi

                if [[ "$IDs" != "$ManagedIDs" ]]; then
                    printf 'Image partition has additional mounts: %sp%s\n' "$LoopDevice" "$Partition" >&2
                    return 1
                fi

                Found=1
            elif (( $? != 1 )); then
                return 1
            fi
        done

        if (( Found == 0 )); then
            printf 'Image is already attached outside the managed mount workflow: %s\n' "$LoopDevice" >&2
            return 1
        fi
    done <<< "$Existing"

    while IFS= read -r LoopDevice; do
        for Partition in 2 1; do
            MountPoint="$MountBase/EFI"

            if (( Partition == 2 )); then
                MountPoint="$MountBase/Root"
            fi

            if findmnt --noheadings --source "${LoopDevice}p${Partition}" --mountpoint "$MountPoint" >/dev/null; then
                if ! MountPrivilege umount "$MountPoint"; then
                    printf 'Cannot unmount %s; image operation stopped.\n' "$MountPoint" >&2
                    return 1
                fi
            fi
        done

        if ! MountPrivilege losetup -d "$LoopDevice"; then
            return 1
        fi
    done <<< "$Existing"
}

MountPrepareQEMU()
{
    local Existing

    if ! Existing=$(losetup --associated "$MountRoot/$DiskImage" --noheadings --output NAME); then
        return 1
    fi

    if [[ -z "$Existing" ]]; then
        return 0
    fi

    if ! MountAuthenticate; then
        return 1
    fi

    MountReleaseManaged
}

MountOpen()
(
    local ImagePath="$MountRoot/$DiskImage"
    local MountBase="$MountRoot/LineOS/Mount"
    local LoopDevice=""
    local MountStage=""
    local ActiveMountBase=""
    local EFIMounted=0
    local RootMounted=0
    local MountPoint Command

    MountOpenCleanup()
    {
        local Status=$?

        trap - EXIT

        if (( RootMounted )); then
            if ! MountPrivilege umount "$ActiveMountBase/Root"; then
                exit 1
            fi
        fi

        if (( EFIMounted )); then
            if ! MountPrivilege umount "$ActiveMountBase/EFI"; then
                exit 1
            fi
        fi

        if [[ -n "$LoopDevice" ]]; then
            if ! MountPrivilege losetup -d "$LoopDevice"; then
                exit 1
            fi
        fi

        if [[ -n "$MountStage" ]]; then
            for MountPoint in "$MountStage/EFI" "$MountStage/Root"; do
                if [[ -d "$MountPoint" ]]; then
                    if ! rmdir -- "$MountPoint"; then
                        Status=1
                    fi
                fi
            done

            if ! rmdir -- "$MountStage"; then
                Status=1
            fi
        fi

        exit "$Status"
    }

    if [[ ! -f "$ImagePath" ]]; then
        printf 'Missing disk image: %s\n' "$ImagePath" >&2
        return 1
    fi

    for Command in losetup findmnt fuser mount umount mktemp chmod mv rmdir; do
        if ! command -v "$Command" >/dev/null 2>&1; then
            printf 'Missing command: %s\n' "$Command" >&2
            return 1
        fi
    done

    if MountPrivilege fuser -s -- "$ImagePath"; then
        printf 'Disk image is in use by another process: %s\n' "$ImagePath" >&2
        return 1
    elif (( $? != 1 )); then
        return 1
    fi

    if ! MountReleaseManaged; then
        return 1
    fi

    for MountPoint in "$MountBase/EFI" "$MountBase/Root"; do
        if findmnt --mountpoint "$MountPoint" >/dev/null; then
            printf 'Mount directory is already in use: %s\n' "$MountPoint" >&2
            return 1
        fi
    done

    if [[ -L "$MountBase" ]]; then
        printf 'Managed mount directory must not be a symbolic link: %s\n' "$MountBase" >&2
        return 1
    fi

    if ! mkdir -p "$MountRoot/LineOS"; then
        return 1
    fi

    if ! MountStage=$(mktemp -d "$MountRoot/LineOS/.Mount.XXXXXXXX"); then
        return 1
    fi

    ActiveMountBase="$MountStage"

    trap MountOpenCleanup EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM

    if ! mkdir -p "$MountStage/EFI" "$MountStage/Root"; then
        return 1
    fi

    if [[ -d "$MountBase" ]]; then
        if ! chmod --reference="$MountBase" "$MountStage"; then
            return 1
        fi
    elif ! chmod 0755 "$MountStage"; then
        return 1
    fi

    if ! LoopDevice=$(MountPrivilege losetup --find --show --partscan "$ImagePath"); then
        return 1
    fi

    if ! MountPrivilege mount -t vfat -o "nosuid,nodev,noexec,uid=$(id -u),gid=$(id -g)" "${LoopDevice}p1" "$MountStage/EFI"; then
        return 1
    fi

    EFIMounted=1

    if ! MountPrivilege mount -t ext2 -o nosuid,nodev "${LoopDevice}p2" "$MountStage/Root"; then
        return 1
    fi

    RootMounted=1

    for MountPoint in "$MountBase/EFI" "$MountBase/Root"; do
        if [[ -e "$MountPoint" || -L "$MountPoint" ]]; then
            if ! rmdir -- "$MountPoint"; then
                printf 'Mount directory is not empty; contents preserved: %s\n' "$MountPoint" >&2
                return 1
            fi
        fi
    done

    trap '' INT TERM

    if ! mv -T -- "$MountStage" "$MountBase"; then
        trap 'exit 130' INT
        trap 'exit 143' TERM
        return 1
    fi

    ActiveMountBase="$MountBase"
    MountStage=""
    trap - EXIT INT TERM

    printf 'EFI mounted at: %s\n' "$MountBase/EFI"
    printf 'Root mounted at: %s\n' "$MountBase/Root"
)

source "$MountDir/Programs.sh"

MountProgramCopy()
{
    local Source="$1"
    local CopyPath="$2"
    local Root="$3"
    local Mode="${4:-0755}"
    local Directory="$Root"
    local Component Destination Target
    local -a Components=()

    if [[ -L "$Source" || ! -f "$Source" || -L "$Root" ]]; then
        return 1
    fi
    if ! Target=$(findmnt --raw --noheadings --mountpoint "$Root" --types ext2 --output TARGET) || [[ "$Target" != "$Root" ]]; then
        return 1
    fi

    IFS=/ read -r -a Components <<< "${CopyPath#/}"
    for Component in "${Components[@]}"; do
        [[ -z "$Component" ]] && continue
        if ! Target=$(findmnt --raw --noheadings --target "$Directory" --output TARGET) || [[ "$Target" != "$Root" ]]; then
            return 1
        fi
        Directory="$Directory/$Component"
        if MountPrivilege test -L "$Directory"; then
            return 1
        fi
        if MountPrivilege test -e "$Directory"; then
            if ! Target=$(findmnt --raw --noheadings --target "$Directory" --output TARGET) || [[ "$Target" != "$Root" ]]; then
                return 1
            fi
        fi
        if ! MountPrivilege install -d -m 0755 -- "$Directory"; then
            return 1
        fi
        if ! Target=$(findmnt --raw --noheadings --target "$Directory" --output TARGET) || [[ "$Target" != "$Root" ]]; then
            return 1
        fi
    done

    Destination="$Directory/${Source##*/}"
    if MountPrivilege test -L "$Destination" || MountPrivilege test -d "$Destination"; then
        return 1
    fi
    if MountPrivilege test -e "$Destination"; then
        if ! MountPrivilege test -f "$Destination" || [[ $(MountPrivilege stat -c %h -- "$Destination") != 1 ]]; then
            return 1
        fi
    fi
    MountPrivilege install -T -o 0 -g 0 -m "$Mode" -- "$Source" "$Destination"
}

MountUpdate()
(
    local ImagePath="$MountRoot/$DiskImage"
    local BootFile="$MountRoot/LineOS/EFI/BOOT/BOOTX64.EFI"
    local KernelFile="$MountRoot/LineOS/KERNEL/LINEOS_KERNEL.ELF"
    local ProgramsDir="$MountRoot/LineOS/Programs"
    local LoopDevice=""
    local MountPoint=""
    local Mounted=0
    local File Command ProgramFile Program CopyPath FontFile
    local -a FontFiles=()
    local -a ProgramPaths=()
    local -a ProgramModes=()
    local -a ProgramFiles=()

    MountCleanup()
    {
        local Status=$?

        trap - EXIT

        if (( Mounted )); then
            if ! MountPrivilege umount "$MountPoint"; then
                printf 'Unmount failed; inspect %s and %s. Loop device retained.\n' "$MountPoint" "$LoopDevice" >&2
                exit 1
            fi
        fi

        if [[ -n "$LoopDevice" ]]; then
            if ! MountPrivilege losetup -d "$LoopDevice"; then
                printf 'Cannot detach %s\n' "$LoopDevice" >&2
                exit 1
            fi
        fi

        if [[ -n "$MountPoint" ]]; then
            if ! rmdir "$MountPoint"; then
                Status=1
            fi
        fi

        exit "$Status"
    }

    for File in "$ImagePath" "$BootFile" "$KernelFile"; do
        if [[ ! -f "$File" ]]; then
            printf 'Missing mount input: %s\n' "$File" >&2
            return 1
        fi
    done

    if [[ ! -d "$ProgramsDir" ]]; then
        printf 'Missing user program directory: %s\n' "$ProgramsDir" >&2
        return 1
    fi

    local Manifest Dependencies Sources Destination ProgramMode
    local -A Destinations=()
    if ! Manifest=$(ProgramManifest "$MountRoot"); then
        return 1
    fi
    while IFS='|' read -r Program CopyPath Dependencies Sources ProgramMode; do
        [[ -z "$Program" ]] && continue
        ProgramFile="$ProgramsDir/$Program/${Program##*/}.lix"
        if [[ -L "$ProgramFile" || ! -f "$ProgramFile" || ! -r "$ProgramFile" ]]; then
            printf 'Missing or invalid program output: %s\n' "$ProgramFile" >&2
            return 1
        fi
        Destination="${CopyPath%/}/${Program##*/}.lix"
        Destinations[$Destination]=1
        ProgramFiles+=("$ProgramFile")
        ProgramPaths+=("$CopyPath")
        ProgramModes+=("${ProgramMode:-0755}")
    done <<< "$Manifest"

    shopt -s nullglob
    FontFiles=("$MountRoot"/Assets/Fonts/*.ttf)
    if (( ${#FontFiles[@]} == 0 )); then
        printf 'No TTF fonts found in: %s\n' "$MountRoot/Assets/Fonts" >&2
        return 1
    fi

    for FontFile in "${FontFiles[@]}"; do
        if [[ -L "$FontFile" || ! -f "$FontFile" || ! -r "$FontFile" ]]; then
            printf 'Invalid font file: %s\n' "$FontFile" >&2
            return 1
        fi
    done

    for FontFile in "${FontFiles[@]}"; do
        Destination="/sys/fonts/${FontFile##*/}"
        ProgramDestinationCheck "$Destination" "${!Destinations[@]}" || return 1
        Destinations[$Destination]=1
    done

    for Command in sudo losetup findmnt mount umount mktemp install stat; do
        if ! command -v "$Command" >/dev/null 2>&1; then
            printf 'Missing command: %s\n' "$Command" >&2
            return 1
        fi
    done

    if ! MountReleaseManaged; then
        return 1
    fi

    trap MountCleanup EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM

    if ! MountPoint=$(mktemp -d); then
        return 1
    fi

    if ! LoopDevice=$(MountPrivilege losetup --find --show --partscan "$ImagePath"); then
        return 1
    fi

    if ! MountPrivilege mount -t vfat -o nosuid,nodev,noexec "${LoopDevice}p1" "$MountPoint"; then
        return 1
    fi

    Mounted=1

    if ! MountPrivilege install -D -m 0644 "$BootFile" "$MountPoint/EFI/BOOT/BOOTX64.EFI"; then
        return 1
    fi

    if ! MountPrivilege install -D -m 0644 "$KernelFile" "$MountPoint/KERNEL/LINEOS_KERNEL.ELF"; then
        return 1
    fi

    if ! MountPrivilege umount "$MountPoint"; then
        return 1
    fi

    Mounted=0

    if ! MountPrivilege mount -t ext2 -o nosuid,nodev "${LoopDevice}p2" "$MountPoint"; then
        return 1
    fi

    Mounted=1

    if ! MountPrivilege test -e "$MountPoint/etc/timezone" && ! MountPrivilege test -L "$MountPoint/etc/timezone"; then
        if ! MountProgramCopy "$MountRoot/Configs/timezone" "/etc" "$MountPoint" 0644; then
            printf 'Timezone configuration copy rejected or failed\n' >&2
            return 1
        fi
    fi

    if ! MountPrivilege test -e "$MountPoint/etc/sudoers" && ! MountPrivilege test -L "$MountPoint/etc/sudoers"; then
        if ! MountProgramCopy "$MountRoot/Configs/sudoers" "/etc" "$MountPoint" 0600; then
            printf 'Sudo policy copy rejected or failed\n' >&2
            return 1
        fi
    fi

    local Index=0

    for ProgramFile in "${ProgramFiles[@]}"; do
        if ! MountProgramCopy "$ProgramFile" "${ProgramPaths[$Index]}" "$MountPoint" "${ProgramModes[$Index]}"; then
            printf 'Program copy rejected or failed: %s\n' "$ProgramFile" >&2
            return 1
        fi
        Index=$((Index + 1))
    done

    for FontFile in "${FontFiles[@]}"; do
        if ! MountProgramCopy "$FontFile" "/sys/fonts" "$MountPoint" 0644; then
            printf 'Font copy rejected or failed: %s\n' "$FontFile" >&2
            return 1
        fi
    done

    if ! MountPrivilege umount "$MountPoint"; then
        return 1
    fi

    Mounted=0
)
