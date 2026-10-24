#!/usr/bin/env bash
# Scripts/Backend/Config.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

ConfigDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ConfigRoot="$ConfigDir/../.."

ConfigLoad()
{
    local File="$ConfigRoot/Configs/QEMUSettings.sh"

    if [[ ! -f "$File" ]]; then
        printf 'Missing configuration: %s\n' "$File" >&2
        return 1
    fi

    if ! source "$File"; then
        return 1
    fi

    if ! ConfigValidate; then
        printf 'Invalid configuration: %s\n' "$File" >&2
        return 1
    fi

    return 0
}

ConfigValidate()
{
    if [[ -z "${MachineType:-}" ]]; then
        return 1
    fi

    if [[ -z "${MachineAcceleration:-}" ]]; then
        return 1
    fi

    if [[ -z "${CPUModel:-}" ]]; then
        return 1
    fi

    if ! [[ "${CPUCount:-}" =~ ^[1-9][0-9]*$ ]]; then
        return 1
    fi

    if [[ -z "${MemorySize:-}" ]]; then
        return 1
    fi

    if [[ -z "${GraphicsDisplay:-}" ]]; then
        return 1
    fi

    if [[ -z "${GraphicsResolution:-}" ]]; then
        return 1
    fi

    if [[ -z "${GraphicsDevice:-}" ]]; then
        return 1
    fi

    if [[ -z "${GraphicsHostMemory:-}" ]]; then
        return 1
    fi

    if [[ -z "${NetworkBackend:-}" ]]; then
        return 1
    fi

    if [[ -z "${NetworkMAC:-}" ]]; then
        return 1
    fi

    if [[ -z "${InputKeyboard:-}" ]]; then
        return 1
    fi

    if [[ -z "${InputMouse:-}" ]]; then
        return 1
    fi

    if [[ -z "${RTCBase:-}" ]]; then
        return 1
    fi

    if [[ -z "${RTCClock:-}" ]]; then
        return 1
    fi

    if [[ -z "${DiskImage:-}" ]]; then
        return 1
    fi

    if [[ -z "${DiskSize:-}" ]]; then
        return 1
    fi

    if ! [[ "${DiskEFISizeMiB:-}" =~ ^[1-9][0-9]*$ ]]; then
        return 1
    fi

    if [[ "${DebugNoReboot:-}" != true && "${DebugNoReboot:-}" != false ]]; then
        return 1
    fi

    if [[ "${DebugNoShutdown:-}" != true && "${DebugNoShutdown:-}" != false ]]; then
        return 1
    fi

    return 0
}
