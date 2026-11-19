#!/usr/bin/env bash
# Scripts/Backend/Libraries.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

LibraryCopySO()
(
    local Root="$1"
    local Library="$2"
    local SO="$3"
    local Path Component Temporary
    local -a Components=()
    [[ "$Library" =~ ^[A-Za-z0-9_-]+$ && "$SO" =~ ^[A-Za-z0-9_][A-Za-z0-9_.-]*(/[A-Za-z0-9_][A-Za-z0-9_.-]*)*\.so$ ]] || return 1
    Path="$Root"
    IFS=/ read -r -a Components <<< "Libraries/$Library/$SO"
    for Component in "${Components[@]}"; do
        Path+="/$Component"
        [[ ! -L "$Path" ]] || { printf 'Symlink in so path: %s\n' "$Path" >&2; return 1; }
    done
    [[ -f "$Path" ]] || { printf 'Missing so output: %s\n' "$Path" >&2; return 1; }
    [[ ! -L "$Root/LineOS" && ! -L "$Root/LineOS/Libraries" ]] || return 1
    mkdir -p -- "$Root/LineOS/Libraries" || return 1
    Temporary=$(mktemp "$Root/LineOS/Libraries/.library.XXXXXXXX") || return 1
    trap 'rm -f -- "$Temporary"' EXIT
    cp -- "$Path" "$Temporary" && chmod 755 "$Temporary" && mv -fT -- "$Temporary" "$Root/LineOS/Libraries/${SO##*/}"
)

LibraryManifest()
(
    set -o pipefail
    local Root="$1"
    local List ListSources Config Library Name Line Key Value Enable CFlags Sources Source IncludedMakefile SO Output Path Component
    local -A Names=() Seen=() Outputs=([liblosc.so]=LOSLibC [ld-lineos-x86_64.so.1]=Loader)
    local -a Components=()
    List=$(find "$Root/Libraries" -mindepth 2 -maxdepth 2 -name Library.cfg -print | LC_ALL=C sort) || return 1
    while IFS= read -r Config; do
        [[ -z "$Config" ]] && continue
        Library=${Config#"$Root/Libraries/"}
        Library=${Library%/Library.cfg}
        if [[ ! "$Library" =~ ^[A-Za-z0-9_-]+$ || -L "$Root/Libraries" || -L "$Root/Libraries/$Library" || -L "$Config" || ! -f "$Config" ]]; then
            printf 'Invalid library: %s\n' "$Config" >&2
            return 1
        fi
        Name=${Library##*/}
        Enable=true
        CFlags=-
        IncludedMakefile=-
        SO=-
        Seen=()
        while IFS= read -r Line || [[ -n "$Line" ]]; do
            Line="${Line#"${Line%%[![:space:]]*}"}"
            Line="${Line%"${Line##*[![:space:]]}"}"
            [[ -z "$Line" || "$Line" == \#* ]] && continue
            [[ "$Line" == *=* ]] || { printf 'Invalid configuration: %s\n' "$Config" >&2; return 1; }
            Key=${Line%%=*}
            Value=${Line#*=}
            Key="${Key#"${Key%%[![:space:]]*}"}"
            Key="${Key%"${Key##*[![:space:]]}"}"
            Value="${Value#"${Value%%[![:space:]]*}"}"
            Value="${Value%"${Value##*[![:space:]]}"}"
            if [[ -z "$Key" || -n "${Seen[$Key]:-}" ]]; then
                printf 'Invalid or duplicate configuration key: %s: %s\n' "$Config" "$Key" >&2
                return 1
            fi
            Seen[$Key]=1
            case "$Key" in
                Enable)
                    [[ "$Value" == true || "$Value" == false ]] || { printf 'Invalid Enable: %s\n' "$Config" >&2; return 1; }
                    Enable=$Value
                    ;;
                IncludedMakefile)
                    if [[ ! "$Value" =~ ^[A-Za-z0-9_][A-Za-z0-9_.-]*(/[A-Za-z0-9_][A-Za-z0-9_.-]*)*$ || "/$Value/" == */../* || "/$Value/" == */./* ]]; then
                        printf 'Invalid IncludedMakefile: %s: %s\n' "$Config" "$Value" >&2
                        return 1
                    fi
                    IncludedMakefile=$Value
                    ;;
                so)
                    [[ "$Value" =~ ^[A-Za-z0-9_][A-Za-z0-9_.-]*(/[A-Za-z0-9_][A-Za-z0-9_.-]*)*\.so$ ]] || { printf 'Invalid so: %s: %s\n' "$Config" "$Value" >&2; return 1; }
                    SO=$Value
                    ;;
                CFlags)
                    if [[ -n "$Value" ]]; then
                        [[ "$Value" =~ ^[-A-Za-z0-9_=+.,/:[:blank:]]+$ ]] || { printf 'Invalid CFlags: %s\n' "$Config" >&2; return 1; }
                        Value=${Value//$'\t'/ }
                        CFlags=${Value// /,}
                    fi
                    ;;
                *) printf 'Unknown configuration key: %s: %s\n' "$Config" "$Key" >&2; return 1 ;;
            esac
        done < "$Config"
        [[ "$Enable" == false ]] && continue
        if [[ "${Name,,}" == loslibc || -n "${Names[${Name,,}]:-}" ]]; then
            printf 'Duplicate or reserved library name: %s\n' "$Name" >&2
            return 1
        fi
        Names[${Name,,}]=1
        if [[ "$SO" != - && "$IncludedMakefile" == - ]]; then
            printf 'so requires IncludedMakefile: %s\n' "$Config" >&2
            return 1
        fi
        Output=-
        if [[ "$SO" != - ]]; then
            Output=${SO##*/}
        elif [[ "$IncludedMakefile" == - ]]; then
            Output="lib${Name,,}.so"
        fi
        if [[ "$Output" != - ]]; then
            [[ -z "${Outputs[$Output]:-}" ]] || { printf 'Duplicate library output: %s\n' "$Output" >&2; return 1; }
            Outputs[$Output]=$Name
        fi
        Sources=-
        if [[ "$IncludedMakefile" != - ]]; then
            Path="$Root/Libraries/$Library"
            IFS=/ read -r -a Components <<< "$IncludedMakefile"
            for Component in "${Components[@]}"; do
                Path+="/$Component"
                if [[ -L "$Path" ]]; then
                    printf 'Symlink in IncludedMakefile: %s: %s\n' "$Config" "$IncludedMakefile" >&2
                    return 1
                fi
            done
            if [[ ! -f "$Root/Libraries/$Library/$IncludedMakefile" ]]; then
                printf 'Missing IncludedMakefile: %s: %s\n' "$Config" "$IncludedMakefile" >&2
                return 1
            fi
        else
            ListSources=$(find "$Root/Libraries/$Library/Source" -type f \( -name '*.c' -o -name '*.S' \) | LC_ALL=C sort) || return 1
            while IFS= read -r Source; do
                [[ -z "$Source" ]] && continue
                Source=${Source#"$Root/"}
                [[ "$Source" =~ ^[A-Za-z0-9_./-]+$ ]] || { printf 'Unsupported source path: %s\n' "$Source" >&2; return 1; }
                if [[ "$Sources" == - ]]; then Sources=$Source; else Sources+=",$Source"; fi
            done <<< "$ListSources"
        fi
        printf '%s|%s|%s|%s|%s|%s\n' "$Name" "$Library" "$CFlags" "$Sources" "$IncludedMakefile" "$SO"
    done <<< "$List"
)

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    if [[ "${1:-}" == --copy-so ]]; then
        LibraryCopySO "$2" "$3" "$4"
    else
        LibraryManifest "${1:-.}"
    fi
fi
