#!/usr/bin/env bash
# Scripts/Backend/Libraries.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

LibraryManifest()
(
    set -o pipefail
    local Root="$1"
    local List Config Library Name Line Key Value Enable CFlags Sources Source Owner
    local -A Names=() Seen=()
    List=$(find "$Root/Libraries" -name Library.cfg -print | LC_ALL=C sort) || return 1
    while IFS= read -r Config; do
        [[ -z "$Config" ]] && continue
        Library=${Config#"$Root/Libraries/"}
        Library=${Library%/Library.cfg}
        if [[ ! "$Library" =~ ^[A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)*$ || -L "$Config" || ! -f "$Config" ]]; then
            printf 'Invalid library: %s\n' "$Config" >&2
            return 1
        fi
        Name=${Library##*/}
        Enable=true
        CFlags=-
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
                CFlags)
                    if [[ -n "$Value" ]]; then
                        [[ "$Value" =~ ^[-A-Za-z0-9_=+.,/:[:space:]]+$ ]] || { printf 'Invalid CFlags: %s\n' "$Config" >&2; return 1; }
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
        Sources=-
        while IFS= read -r Source; do
            Owner=${Source%/*}
            while [[ "$Owner" != "$Root/Libraries/$Library" && ! -f "$Owner/Library.cfg" ]]; do
                Owner=${Owner%/*}
            done
            [[ "$Owner" != "$Root/Libraries/$Library" ]] && continue
            Source=${Source#"$Root/"}
            [[ "$Source" =~ ^[A-Za-z0-9_./-]+$ ]] || { printf 'Unsupported source path: %s\n' "$Source" >&2; return 1; }
            if [[ "$Sources" == - ]]; then Sources=$Source; else Sources+=",$Source"; fi
        done < <(find "$Root/Libraries/$Library" -type f \( -name '*.c' -o -name '*.S' \) | LC_ALL=C sort)
        printf '%s|%s|%s|%s\n' "$Name" "$Library" "$CFlags" "$Sources"
    done <<< "$List"
)

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    LibraryManifest "${1:-.}"
fi
