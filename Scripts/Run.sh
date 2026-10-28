#!/usr/bin/env bash
# Scripts/Run.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

set -o pipefail

if (( EUID == 0 )); then
    if [[ -z "${SUDO_USER:-}" || "$SUDO_USER" == root ]]; then
        printf 'Run this script as your desktop user; sudo is requested only for disk operations.\n' >&2
        exit 1
    fi

    exec sudo -u "$SUDO_USER" -- bash "${BASH_SOURCE[0]}" "$@"
fi

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

source "$ScriptDir/Backend/Config.sh"
source "$ScriptDir/Backend/Build.sh"
source "$ScriptDir/Backend/Image.sh"
source "$ScriptDir/Backend/Mount.sh"
source "$ScriptDir/Backend/QEMU.sh"

RunClock()
{
    local Uptime

    if ! read -r Uptime < /proc/uptime; then
        return 1
    fi

    Uptime=${Uptime%% *}
    RunNow=$((10#${Uptime%.*} * 100 + 10#${Uptime#*.}))
}

RunDuration()
{
    local Elapsed="$1"

    printf -v RunStageTime '%d.%02d' "$((Elapsed / 100))" "$((Elapsed % 100))"
}

UIEmit()
{
    printf '%s\0' "$#" "$@" >&3
}

UIStart()
{
    if ! command -v node >/dev/null 2>&1; then
        printf 'Node.js is required for the builder UI.\n' >&2
        return 1
    fi

    exec 3> >(node "$ScriptDir/Frontend/UI.js")
    UIPID=$!
}

UIStop()
{
    local Status=$?
    local UIStatus

    trap - EXIT

    if [[ "${MountAfterQEMU:-0}" == 1 ]]; then
        if ! RestoreQEMUMount; then
            if (( Status == 0 )); then
                Status=1
            fi
        fi
    fi
    exec 3>&-
    wait "$UIPID"
    UIStatus=$?

    if (( Status == 0 && UIStatus != 0 )); then
        Status=$UIStatus
    fi

    exit "$Status"
}

UIFlush()
{
    local AckFile

    if ! AckFile=$(mktemp); then
        return 1
    fi

    UIEmit sync "$AckFile"

    while [[ ! -s "$AckFile" ]]; do
        if ! kill -0 "$UIPID" 2>/dev/null; then
            rm -f -- "$AckFile"
            return 1
        fi

        sleep 0.01
    done

    rm -f -- "$AckFile"
}

UIHeader()
{
    UIEmit header "${1:-LineOS Builder}"
}

UIMessage()
{
    UIEmit message "$@"
}

UIHelp()
{
    UIEmit help
}

UISummary()
{
    UIEmit summary "$@"
}

UIErrorFile()
{
    local File="$1"

    if [[ -s "$File" ]]; then
        UIEmit error-text "$(cat "$File")"$'\n'
    fi
}

UIQEMUAnalysis()
{
    local Status="$1"
    local Duration="$2"
    local Rows=()

    if [[ -s "$QEMUSessionFile" ]]; then
        mapfile -t Rows < <(QEMUAnalyze)
    else
        Rows=("Log analysis|Unavailable")
    fi

    UIEmit analysis "$Status" "$Duration" "${Rows[@]}"
}

RestoreQEMUMount()
{
    MountAfterQEMU=0

    printf 'Restoring image mounts...\n'

    if ! MountAuthenticate; then
        return 1
    fi

    if ! MountOpen > "$ScriptDir/../Logs/Remount.log" 2>&1; then
        cat "$ScriptDir/../Logs/Remount.log" >&2
        printf 'Image remount failed. See Logs/Remount.log.\n' >&2
        return 1
    fi

    printf 'EFI mounted: %s/LineOS/Mount/EFI\n' "$MountRoot"
    printf 'Root mounted: %s/LineOS/Mount/Root\n' "$MountRoot"
}

RunMountedQEMU()
{
    local Status
    local Interrupted=0
    local INTTrap
    local TERMTrap

    if ! UIFlush; then
        return 1
    fi

    if ! MountAuthenticate; then
        return 1
    fi

    if ! MountReleaseManaged; then
        return 1
    fi

    INTTrap=$(trap -p INT)
    TERMTrap=$(trap -p TERM)
    trap 'Interrupted=130' INT
    trap 'Interrupted=143' TERM
    MountAfterQEMU=1

    if UIRunQEMU; then
        Status=0
    else
        Status=$?
    fi

    if (( Interrupted != 0 )); then
        Status=$Interrupted
    fi

    if ! RestoreQEMUMount; then
        if (( Status == 0 )); then
            Status=1
        fi
    fi

    trap - INT TERM

    if [[ -n "$INTTrap" ]]; then
        eval "$INTTrap"
    fi

    if [[ -n "$TERMTrap" ]]; then
        eval "$TERMTrap"
    fi

    return "$Status"
}

UIRunQEMU()
(
    local ErrorFile QEMUSessionFile Status Check Start
    local Completed=0
    local Interrupted=0
    local Checks=(Binary Firmware Disk Acceleration)

    if ! ErrorFile=$(mktemp "$ScriptDir/../Logs/.qemu-error.XXXXXXXX"); then
        return 1
    fi

    QEMUSessionFile="$ErrorFile.session"

    trap 'rm -f -- "$ErrorFile" "$QEMUSessionFile"' EXIT
    trap 'Interrupted=130' INT
    trap 'Interrupted=143' TERM

    UIHeader QEMU

    for Check in "${Checks[@]}"; do
        if QEMUCheck "$Check" 2> "$ErrorFile"; then
            Completed=$((Completed + 1))
        else
            Status=$?
            UIErrorFile "$ErrorFile"
            UIMessage error "QEMU check failed: $Check"
            UIQEMUAnalysis "$Status" 0.00
            return "$Status"
        fi

        UIEmit checks "$Completed" "${#Checks[@]}"
    done

    if ! RunClock; then
        return 1
    fi

    Start=$RunNow
    UIEmit qemu-start

    if (( Interrupted != 0 )); then
        return "$Interrupted"
    fi

    if QEMURun 2> "$ErrorFile"; then
        Status=0
    else
        Status=$?
    fi

    if (( Interrupted != 0 )); then
        Status=$Interrupted
    fi

    RunClock
    RunDuration "$((RunNow - Start))"
    UIEmit qemu-end "$Status" "$RunStageTime"

    if (( Status != 0 )); then
        UIErrorFile "$ErrorFile"
    fi

    UIQEMUAnalysis "$Status" "$RunStageTime"

    return "$Status"
)

RunStage()
{
    local Number="$1"
    local Total="$2"
    local Message="$3"
    local Function="$4"
    local LogFile="$ScriptDir/../Logs/$5"
    local Start End Status
    local RunProgressFile
    local RunFileTotal=1
    local RunFileCurrent=0
    local RunFileCompleted=0
    local RunFileName="$Message"

    if ! : > "$LogFile"; then
        UIMessage error "Cannot write $LogFile"
        return 1
    fi

    if ! RunProgressFile=$(mktemp "$ScriptDir/../Logs/.progress.XXXXXXXX"); then
        return 1
    fi

    if ! RunClock; then
        rm -f -- "$RunProgressFile"
        return 1
    fi

    Start=$RunNow
    UIEmit stage-start "$RunProgressFile" "$Message"

    if "$Function" > "$LogFile" 2>&1; then
        Status=0
    else
        Status=$?
    fi

    if [[ -s "$RunProgressFile" ]]; then
        IFS=$'\t' read -r RunFileTotal RunFileCurrent RunFileCompleted RunFileName < "$RunProgressFile"
    fi

    rm -f -- "$RunProgressFile" "$RunProgressFile.next"

    if (( Status == 0 && RunFileTotal == 1 && RunFileCurrent == 0 )); then
        RunFileCurrent=1
    fi

    if ! RunClock; then
        return 1
    fi

    End=$RunNow
    RunStageElapsed=$((End - Start))
    RunDuration "$RunStageElapsed"
    UIEmit stage-end "$Status" "$RunStageTime" "$RunFileTotal" "$RunFileCurrent" "$RunFileCompleted" "$RunFileName"

    if (( Status != 0 )); then
        UIMessage error "Exit $Status · $LogFile"
        UIEmit error-text "$(tail -n 20 "$LogFile")"$'\n'
    fi

    return "$Status"
}

RunMain()
{
    local Option="${1:-all}"
    Option="${Option,,}"
    Option="${Option#-}"
    Option="${Option#-}"

    case "$Option" in
        h|help)
            UIHelp
            return 0
            ;;
        clean)
            UIHeader

            if ! mkdir -p "$ScriptDir/../Logs"; then
                return 1
            fi

            if ! RunStage 1 1 "Cleaning build files" BuildClean Clean.log; then
                return 1
            fi

            UIMessage success "Clean completed"
            return 0
            ;;
        format)
            UIHeader

            if ! mkdir -p "$ScriptDir/../Logs"; then
                return 1
            fi

            if ! RunStage 1 1 "Formatting sources" BuildFormat Format.log; then
                return 1
            fi

            UIMessage success "Format completed"
            return 0
            ;;
        build)
            local BuildProgram="${2:-}"
            UIHeader

            if ! ConfigLoad; then
                UIMessage error "Failed to load configuration" >&2
                return 1
            fi

            if ! mkdir -p "$ScriptDir/../Logs"; then
                return 1
            fi

            if ! RunStage 1 1 "Building LineOS" BuildRun Build.log; then
                return 1
            fi

            UIMessage success "Build completed"
            return 0
            ;;
        image)
            UIHeader

            if ! ConfigLoad; then
                UIMessage error "Failed to load configuration" >&2
                return 1
            fi

            if ! mkdir -p "$ScriptDir/../Logs"; then
                return 1
            fi

            if ! UIFlush; then
                return 1
            fi

            if ! MountAuthenticate; then
                return 1
            fi

            if ! RunStage 1 2 "Preparing disk image" ImageCreate Image.log; then
                return 1
            fi

            if ! RunStage 2 2 "Updating boot files" MountUpdate Mount.log; then
                return 1
            fi

            UIMessage success "Image update completed"
            return 0
            ;;
        m|mount)
            UIHeader

            if ! ConfigLoad; then
                UIMessage error "Failed to load configuration" >&2
                return 1
            fi

            if ! mkdir -p "$ScriptDir/../Logs"; then
                return 1
            fi

            if ! UIFlush; then
                return 1
            fi

            if ! MountAuthenticate; then
                return 1
            fi

            if ! RunStage 1 1 "Mounting disk image" MountOpen Mount.log; then
                return 1
            fi

            UIMessage success "EFI mounted: $MountRoot/LineOS/Mount/EFI"
            UIMessage success "Root mounted: $MountRoot/LineOS/Mount/Root"
            return 0
            ;;
        u|umount|unmount)
            UIHeader

            if ! ConfigLoad; then
                UIMessage error "Failed to load configuration" >&2
                return 1
            fi

            if ! UIFlush; then
                return 1
            fi

            if ! MountAuthenticate; then
                return 1
            fi

            if ! MountReleaseManaged; then
                return 1
            fi

            UIMessage success "Image unmounted and loop devices detached"
            return 0
            ;;
        qemu)
            UIHeader

            if ! ConfigLoad; then
                UIMessage error "Failed to load configuration" >&2
                return 1
            fi

            if ! mkdir -p "$ScriptDir/../Logs"; then
                return 1
            fi

            RunMountedQEMU
            return $?
            ;;
        all|"")
            ;;
        *)
            UIMessage error "Unknown option: $1" >&2
            UIEmit error-text 'Use ./Scripts/Run.sh --help for available options.'$'\n'
            return 1
            ;;
    esac

    local BuildTime ImageTime MountTime TotalTime
    local TotalElapsed=0

    UIHeader

    if ! ConfigLoad; then
        UIMessage error "Failed to load configuration" >&2
        return 1
    fi

    if ! mkdir -p "$ScriptDir/../Logs"; then
        return 1
    fi

    if ! UIFlush; then
        return 1
    fi

    if ! MountAuthenticate; then
        return 1
    fi

    if ! MountReleaseManaged; then
        return 1
    fi

    if ! RunStage 1 3 "Building LineOS" BuildRun Build.log; then
        return 1
    fi

    BuildTime=$RunStageTime
    TotalElapsed=$((TotalElapsed + RunStageElapsed))

    if ! UIFlush; then
        return 1
    fi

    if ! MountAuthenticate; then
        return 1
    fi

    if ! RunStage 2 3 "Preparing disk image" ImageCreate Image.log; then
        return 1
    fi

    ImageTime=$RunStageTime
    TotalElapsed=$((TotalElapsed + RunStageElapsed))

    if ! RunStage 3 3 "Updating boot files" MountUpdate Mount.log; then
        return 1
    fi

    MountTime=$RunStageTime
    TotalElapsed=$((TotalElapsed + RunStageElapsed))
    RunDuration "$TotalElapsed"
    TotalTime=$RunStageTime

    UISummary "$BuildTime" "$ImageTime" "$MountTime" "$TotalTime"
    RunMountedQEMU
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    if ! UIStart; then
        exit 1
    fi

    trap UIStop EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    RunMain "$@"
fi
