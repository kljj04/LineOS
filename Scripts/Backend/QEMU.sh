#!/usr/bin/env bash
# Scripts/Backend/QEMU.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

QEMUDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
QEMURoot="$(cd -- "$QEMUDir/../.." && pwd)"

QEMUCheck()
{
    local Check="$1"
    local File

    case "$Check" in
        Binary)
            if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
                printf 'qemu-system-x86_64 not found.\n' >&2
                return 1
            fi

            ;;
        Firmware)
            for File in "$QEMURoot/UEFI/OVMF_CODE.fd" "$QEMURoot/UEFI/OVMF_VARS.fd"; do
                if [[ ! -r "$File" ]]; then
                    printf 'Unreadable firmware: %s\n' "$File" >&2
                    return 1
                fi
            done

            ;;
        Disk)
            File="$QEMURoot/$DiskImage"

            if ! [[ -f "$File" && -r "$File" && -w "$File" ]]; then
                printf 'Disk image must be readable and writable: %s\n' "$File" >&2
                return 1
            fi

            ;;
        Acceleration)
            if [[ "$MachineAcceleration" == kvm ]]; then
                if ! [[ -r /dev/kvm && -w /dev/kvm ]]; then
                    printf 'KVM is not accessible to the current user.\n' >&2
                    return 1
                fi
            fi

            ;;
        *)
            return 1
            ;;
    esac
}

QEMUAnalyze()
{
    local LogDir="$QEMURoot/Logs"
    local File Bytes

    if [[ -r "$LogDir/QEMU.log" ]]; then
        awk '
            BEGIN {
                Reads = 0
                Writes = 0
                Faults = 0
                Resets = 0
            }

            /Invalid read at addr/ {
                Reads++
            }

            /Invalid write at addr/ {
                Writes++
            }

            {
                Text = tolower($0)

                if (Text ~ /triple fault/)
                {
                    Faults++
                }

                if ($0 ~ /CPU Reset/)
                {
                    Resets++
                }
            }

            END {
                printf "Invalid reads|%d\nInvalid writes|%d\nTriple faults|%d\nCPU reset logs|%d\n", Reads, Writes, Faults, Resets
            }
        ' "$LogDir/QEMU.log"
    fi

    for File in DebugCon.log QEMUProcess.log; do
        if Bytes=$(stat -c %s -- "$LogDir/$File" 2>/dev/null); then
            case "$File" in
                DebugCon.log)
                    printf 'Debug bytes|%s B\n' "$Bytes"
                    ;;
                QEMUProcess.log)
                    printf 'Process bytes|%s B\n' "$Bytes"
                    ;;
            esac
        fi
    done
}

QEMURun()
(
    if (( EUID == 0 )); then
        printf 'QEMU must run as the desktop user.\n' >&2
        return 1
    fi

    local ImagePath="$QEMURoot/$DiskImage"
    local OVMFCode="$QEMURoot/UEFI/OVMF_CODE.fd"
    local OVMFVars="$QEMURoot/UEFI/OVMF_VARS.fd"
    local CPUOptions="$CPUModel"
    local DebugOptions=""
    local Flag
    local Options=()

    local File TempDir Status
    local QEMUPID=""
    local Interrupted=0
    local LogDir="$QEMURoot/Logs"

    for File in "$ImagePath" "$OVMFCode" "$OVMFVars"; do
        if [[ ! -r "$File" ]]; then
            printf 'Missing or unreadable QEMU input: %s\n' "$File" >&2
            return 1
        fi
    done

    local Attached

    if ! Attached=$(losetup --associated "$ImagePath" --noheadings --output NAME); then
        return 1
    fi

    if [[ -n "$Attached" ]]; then
        printf 'Disk image is still attached on the host; unmount it before starting QEMU.\n' >&2
        return 1
    fi

    if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
        printf 'qemu-system-x86_64 not found.\n' >&2
        return 1
    fi

    if ! [[ "$GraphicsResolution" =~ ^[1-9][0-9]*x[1-9][0-9]*$ ]]; then
        printf 'Invalid graphics resolution: %s\n' "$GraphicsResolution" >&2
        return 1
    fi

    if ! mkdir -p "$LogDir"; then
        return 1
    fi

    for File in QEMU.log DebugCon.log QEMUProcess.log; do
        if ! : > "$LogDir/$File"; then
            printf 'Cannot write QEMU log: %s\n' "$LogDir/$File" >&2
            return 1
        fi
    done

    if [[ -n "${QEMUSessionFile:-}" ]]; then
        if ! printf 'ready\n' > "$QEMUSessionFile"; then
            return 1
        fi
    fi

    if ! TempDir=$(mktemp -d); then
        return 1
    fi

    trap 'rm -rf -- "$TempDir"' EXIT
    if ! cp -- "$OVMFVars" "$TempDir/OVMF_VARS.fd"; then
        return 1
    fi

    OVMFVars="$TempDir/OVMF_VARS.fd"

    for Flag in "${CPUFlags[@]}"; do
        CPUOptions+=",$Flag"
    done

    DebugOptions=$(
        IFS=,
        printf '%s' "${DebugFlags[*]}"
    )

    Options+=(
        -D "$LogDir/QEMU.log"
        -debugcon "file:$LogDir/DebugCon.log"
        -global isa-debugcon.iobase=0xe9
        -serial none
        -monitor none
        -machine "$MachineType,accel=$MachineAcceleration"
        -cpu "$CPUOptions"
        -smp "$CPUCount"
        -m "$MemorySize"
        -rtc "base=$RTCBase,clock=$RTCClock"
        -drive "if=none,id=drive0,file=$ImagePath,format=raw"
        -device "virtio-blk-pci,drive=drive0,bootindex=0"
        -vga none
        -device "$GraphicsDevice,xres=${GraphicsResolution%x*},yres=${GraphicsResolution#*x},hostmem=$GraphicsHostMemory"
        -fw_cfg "name=opt/org.tianocore/GraphicsResolution,string=$GraphicsResolution"
        -display "$GraphicsDisplay"
        -device "$InputKeyboard"
        -device "$InputMouse"
        -netdev "$NetworkBackend,id=net0"
        -device "virtio-net-pci,netdev=net0,mac=$NetworkMAC"
        -drive "if=pflash,format=raw,readonly=on,file=$OVMFCode"
        -drive "if=pflash,format=raw,file=$OVMFVars"
    )

    if [[ -n "$DebugOptions" ]]; then
        Options+=(-d "$DebugOptions")
    fi

    if [[ "$DebugNoReboot" == true ]]; then
        Options+=(-no-reboot)
    fi

    if [[ "$DebugNoShutdown" == true ]]; then
        Options+=(-no-shutdown)
    fi

    QEMUStop()
    {
        Interrupted="$1"

        if [[ -n "$QEMUPID" ]]; then
            kill -TERM "$QEMUPID" 2>/dev/null

            while kill -0 "$QEMUPID" 2>/dev/null; do
                wait "$QEMUPID" 2>/dev/null
            done
        fi
    }

    trap 'QEMUStop 130' INT
    trap 'QEMUStop 143' TERM

    qemu-system-x86_64 "${Options[@]}" > "$LogDir/QEMUProcess.log" 2>&1 &
    QEMUPID=$!

    if (( Interrupted != 0 )); then
        QEMUStop "$Interrupted"
    fi

    if wait "$QEMUPID"; then
        Status=0
    else
        Status=$?
    fi

    QEMUPID=""
    trap - INT TERM

    if (( Interrupted != 0 )); then
        Status=$Interrupted
    fi

    if (( Status != 0 )); then
        printf 'QEMU failed (exit %s). See %s/QEMUProcess.log\n' "$Status" "$LogDir" >&2
        tail -n 20 "$LogDir/QEMUProcess.log" >&2
    fi

    return "$Status"
)
