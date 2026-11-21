#!/usr/bin/env bash

ConfigDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ConfigRoot="$(cd -- "$ConfigDir/../.." && pwd)"

ConfigPath()
{
    case "$2" in
        /*) printf '%s\n' "$2" ;;
        *) printf '%s/%s\n' "$1" "$2" ;;
    esac
}

ConfigArray()
{
    local Declaration

    Declaration=$(declare -p "$1" 2>/dev/null) || return 1
    [[ "$Declaration" == "declare -a "* ]]
}

ConfigLoad()
{
    local File="$ConfigRoot/Configs/QEMUSettings.sh"

    if [[ ! -f "$File" ]]; then
        printf 'Missing configuration: %s\n' "$File" >&2
        return 1
    fi
    source "$File" || return 1
    ConfigValidate
}

ConfigValidate()
{
    [[ -n "${QEMUBinary:-}" && -n "${FirmwareCode:-}" && -n "${FirmwareVars:-}" && -n "${DebugLogDir:-}" && -n "${DebugLogFile:-}" && -n "${DebugConsoleFile:-}" && -n "${DebugProcessFile:-}" ]] || return 1
    [[ -n "${DiskImage:-}" && -n "${DiskSize:-}" && "${DiskEFISizeMiB:-}" =~ ^[1-9][0-9]*$ ]] || return 1
    [[ -n "${Machine:-}" && -n "${CPU:-}" && -n "${CPUCount:-}" && -n "${Memory:-}" ]] || return 1
    [[ "${DebugNoReboot:-}" =~ ^(true|false)$ && "${DebugNoShutdown:-}" =~ ^(true|false)$ ]] || return 1
    case "${NetworkBackend:-}" in
        user,*|bridge,*|none) ;;
        *) return 1 ;;
    esac
}
