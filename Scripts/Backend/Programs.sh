#!/usr/bin/env bash
# Scripts/Backend/Programs.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

source "$(dirname "${BASH_SOURCE[0]}")/Libraries.sh"

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
{
    python3 "$(dirname "${BASH_SOURCE[0]}")/ProgramConfig.py" manifest "$1"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    ProgramManifest "${1:-.}"
fi
