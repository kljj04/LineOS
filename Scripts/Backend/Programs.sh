#!/usr/bin/env bash
# Scripts/Backend/Programs.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

source "$(dirname "${BASH_SOURCE[0]}")/Libraries.sh"

ProgramTrim()
{
    local Value="$1"
    Value="${Value#"${Value%%[![:space:]]*}"}"
    Value="${Value%"${Value##*[![:space:]]}"}"
    printf '%s' "$Value"
}

ProgramRead()
{
    local Config="$1"
    local Line Key Value Dependency
    local -A Seen=()
    local -a Items=()
    ProgramCopyPath=/bin
    ProgramDependencies=-
    ProgramEnable=true
    ProgramMode=0755

    while IFS= read -r Line || [[ -n "$Line" ]]; do
        Line=$(ProgramTrim "$Line")
        [[ -z "$Line" || "$Line" == \#* ]] && continue
        if [[ "$Line" != *=* ]]; then
            printf 'Invalid configuration: %s: %s\n' "$Config" "$Line" >&2
            return 1
        fi
        Key=$(ProgramTrim "${Line%%=*}")
        Value=$(ProgramTrim "${Line#*=}")
        if [[ -z "$Key" ]]; then
            printf 'Empty configuration key: %s\n' "$Config" >&2
            return 1
        fi
        if [[ -n "${Seen[$Key]:-}" ]]; then
            printf 'Duplicate configuration key: %s: %s\n' "$Config" "$Key" >&2
            return 1
        fi
        Seen[$Key]=1
        case "$Key" in
            CopyPath)
                if [[ ! "$Value" =~ ^/([A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]*$ || "$Value" == *//* || "/${Value#/}/" == */../* || "/${Value#/}/" == */./* ]]; then
                    printf 'Invalid CopyPath: %s: %s\n' "$Config" "$Value" >&2
                    return 1
                fi
                ProgramCopyPath=${Value%/}
                ;;
            Dependencies)
                ProgramDependencies=-
                if [[ -n "$Value" ]]; then
                    if [[ "$Value" == ,* || "$Value" == *, ]]; then
                        return 1
                    fi
                    IFS=, read -r -a Items <<< "$Value"
                    for Dependency in "${Items[@]}"; do
                        Dependency=$(ProgramTrim "$Dependency")
                        case "$Dependency" in
                            LOSLibC) ;;
                            *)
                                if [[ -z "${ProgramLibraryNames[$Dependency]:-}" ]]; then
                                    printf 'Unknown or disabled dependency: %s: %s\n' "$Config" "$Dependency" >&2
                                    return 1
                                fi
                                ;;
                        esac
                        if [[ "$ProgramDependencies" == - ]]; then
                            ProgramDependencies=$Dependency
                        else
                            ProgramDependencies+=",$Dependency"
                        fi
                    done
                fi
                ;;
            Mode)
                if [[ ! "$Value" =~ ^[0-7]{4}$ ]]; then
                    printf 'Invalid Mode: %s: %s\n' "$Config" "$Value" >&2
                    return 1
                fi
                ProgramMode=$Value
                ;;
            Enable)
                if [[ "$Value" != true && "$Value" != false ]]; then
                    printf 'Invalid Enable: %s\n' "$Config" >&2
                    return 1
                fi
                ProgramEnable=$Value
                ;;
            *) printf 'Unknown configuration key: %s: %s\n' "$Config" "$Key" >&2; return 1 ;;
        esac
    done < "$Config"
}

ProgramDestinationCheck()
{
    local Destination="$1"
    local Existing
    shift
    for Existing in "$@"; do
        if [[ "$Destination" == "$Existing" || "$Destination" == "$Existing/"* || "$Existing" == "$Destination/"* ]]; then
            printf 'Installation destination collision: %s, %s\n' "$Existing" "$Destination" >&2
            return 1
        fi
    done
}

ProgramManifest()
(
    set -o pipefail
    local Root="$1"
    local Config Program Name Destination Sources Source Owner List
    local -A Destinations=() ProgramLibraryNames=()
    local LibraryList LibraryRow
    LibraryList=$(LibraryManifest "$Root") || return 1
    while IFS= read -r LibraryRow; do
        [[ -z "$LibraryRow" ]] && continue
        ProgramLibraryNames[${LibraryRow%%|*}]=1
    done <<< "$LibraryList"
    if ! List=$(find "$Root/UserPrograms" -name Program.cfg -print | LC_ALL=C sort); then
        return 1
    fi
    while IFS= read -r Config; do
        [[ -z "$Config" ]] && continue
        Program=${Config#"$Root/UserPrograms/"}
        Program=${Program%/Program.cfg}
        if [[ ! "$Program" =~ ^[A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)*$ || -L "$Config" || ! -f "$Config" ]]; then
            printf 'Invalid program: %s\n' "$Config" >&2
            return 1
        fi
        ProgramRead "$Config" || return 1
        [[ "$ProgramEnable" == false ]] && continue
        Name=${Program##*/}
        Destination="$ProgramCopyPath/$Name.lix"
        ProgramDestinationCheck "$Destination" "${!Destinations[@]}" || return 1
        Destinations[$Destination]=$Program
        Sources=-
        while IFS= read -r Source; do
            Owner=${Source%/*}
            while [[ "$Owner" != "$Root/UserPrograms/$Program" && ! -f "$Owner/Program.cfg" ]]; do
                Owner=${Owner%/*}
            done
            [[ "$Owner" != "$Root/UserPrograms/$Program" ]] && continue
            Source=${Source#"$Root/"}
            if [[ ! "$Source" =~ ^[A-Za-z0-9_./-]+$ ]]; then
                printf 'Unsupported source path: %s\n' "$Source" >&2
                return 1
            fi
            if [[ "$Sources" == - ]]; then Sources=$Source; else Sources+=",$Source"; fi
        done < <(find "$Root/UserPrograms/$Program" -type f \( -name '*.c' -o -name '*.S' \) | LC_ALL=C sort)
        printf '%s|%s|%s|%s|%s\n' "$Program" "${ProgramCopyPath:-/}" "$ProgramDependencies" "$Sources" "$ProgramMode"
    done <<< "$List"
)

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    ProgramManifest "${1:-.}"
fi
