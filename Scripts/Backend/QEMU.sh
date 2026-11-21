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
            if ! command -v "$QEMUBinary" >/dev/null 2>&1; then
                printf 'Configured QEMU binary not found.\n' >&2
                return 1
            fi

            ;;
        Firmware)
            for File in "$(ConfigPath "$QEMURoot" "$FirmwareCode")" "$(ConfigPath "$QEMURoot" "$FirmwareVars")"; do
                if [[ ! -r "$File" ]]; then
                    printf 'Unreadable firmware: %s\n' "$File" >&2
                    return 1
                fi
            done

            ;;
        Disk)
            File=$(ConfigPath "$QEMURoot" "$DiskImage") || return 1

            if ! [[ -f "$File" && -r "$File" && -w "$File" ]]; then
                printf 'Disk image must be readable and writable: %s\n' "$File" >&2
                return 1
            fi

            ;;
        Acceleration)
            if [[ ",$Machine," == *,accel=kvm,* ]]; then
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
    local LogDir
    LogDir=$(ConfigPath "$QEMURoot" "$DebugLogDir") || return 1
    local File Bytes

    if [[ -r "$LogDir/$DebugLogFile" ]]; then
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
        ' "$LogDir/$DebugLogFile"
    fi

    for File in "$DebugConsoleFile" "$DebugProcessFile"; do
        if Bytes=$(stat -c %s -- "$LogDir/$File" 2>/dev/null); then
            case "$File" in
                "$DebugConsoleFile")
                    printf 'Debug bytes|%s B\n' "$Bytes"
                    ;;
                "$DebugProcessFile")
                    printf 'Process bytes|%s B\n' "$Bytes"
                    ;;
            esac
        fi
    done
}

QEMUBuildOptions()
{
    local ImagePath
    local CodePath="$1"
    local VarsPath="$2"
    local LogDir="$3"
    local Argument
    local DebugLogPath="$LogDir/$DebugLogFile"
    local DebugConsolePath="$LogDir/$DebugConsoleFile"

    ConfigValidate || return 1
    ImagePath=$(ConfigPath "$QEMURoot" "$DiskImage") || return 1
    ImagePath=${ImagePath//,/,,}
    CodePath=${CodePath//,/,,}
    VarsPath=${VarsPath//,/,,}
    local -a RawOptions=()

    if [[ -n "$Debug" ]]; then
        RawOptions+=(-D "$Debug")
    fi
    if [[ -n "$DebugConsole" ]]; then
        RawOptions+=(-debugcon "$DebugConsole")
    fi
    if [[ -n "$DebugGlobal" ]]; then
        RawOptions+=(-global "$DebugGlobal")
    fi
    RawOptions+=(-serial "$Serial" -monitor "$Monitor" -machine "$Machine" -cpu "$CPU" -smp "$CPUCount" -m "$Memory" -rtc "$RTC" -drive "$DiskDrive" -device "$DiskDevice" -vga "$GraphicsVGA" -device "$GraphicsDevice" -fw_cfg "$GraphicsFirmware" -display "$GraphicsDisplay" -device "$InputKeyboard" -device "$InputMouse")
    if [[ "$NetworkBackend" == none ]]; then
        RawOptions+=(-nic none)
    else
        RawOptions+=(-netdev "$NetworkBackend" -device "$NetworkDevice")
    fi
    RawOptions+=(-drive "$FirmwareCodeDrive" -drive "$FirmwareVarsDrive")
    if [[ -n "$DebugFlags" ]]; then
        RawOptions+=(-d "$DebugFlags")
    fi
    if [[ "$DebugNoReboot" == true ]]; then
        RawOptions+=(-no-reboot)
    fi
    if [[ "$DebugNoShutdown" == true ]]; then
        RawOptions+=(-no-shutdown)
    fi
    Options=()
    for Argument in "${RawOptions[@]}"; do
        Argument=${Argument//\{DiskImage\}/"$ImagePath"}
        Argument=${Argument//\{FirmwareCode\}/"$CodePath"}
        Argument=${Argument//\{FirmwareVars\}/"$VarsPath"}
        Argument=${Argument//\{DebugLogPath\}/"$DebugLogPath"}
        Argument=${Argument//\{DebugConsolePath\}/"$DebugConsolePath"}
        Options+=("$Argument")
    done
}

QEMURun()
(
    if (( EUID == 0 )); then
        printf 'QEMU must run as the desktop user.\n' >&2
        return 1
    fi

    local ImagePath
    ImagePath=$(ConfigPath "$QEMURoot" "$DiskImage") || return 1
    local OVMFCode="$(ConfigPath "$QEMURoot" "$FirmwareCode")"
    local OVMFVars="$(ConfigPath "$QEMURoot" "$FirmwareVars")"
    local Options=()

    local File TempDir Status
    local QEMUPID=""
    local Interrupted=0
    local LogDir
    LogDir=$(ConfigPath "$QEMURoot" "$DebugLogDir") || return 1

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

    if ! command -v "$QEMUBinary" >/dev/null 2>&1; then
        printf 'Configured QEMU binary not found.\n' >&2
        return 1
    fi

    if ! mkdir -p "$LogDir"; then
        return 1
    fi

    for File in "$DebugLogFile" "$DebugConsoleFile" "$DebugProcessFile"; do
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

    QEMUBuildOptions "$OVMFCode" "$OVMFVars" "$LogDir" || return 1

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

    "$QEMUBinary" "${Options[@]}" > "$LogDir/$DebugProcessFile" 2>&1 &
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
        printf 'QEMU failed (exit %s). See %s/%s\n' "$Status" "$LogDir" "$DebugProcessFile" >&2
        tail -n 20 "$LogDir/$DebugProcessFile" >&2
    fi

    return "$Status"
)
