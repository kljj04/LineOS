#!/usr/bin/env bash
# Scripts/Backend/Libraries.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

LibraryManifest()
{
    python3 "${BASH_SOURCE[0]%/*}/LibraryConfig.py" "$1"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    if [[ "${1:-}" == --copy-so ]]; then
        python3 "${BASH_SOURCE[0]%/*}/LibraryConfig.py" --copy "$2" "$3"
    else
        LibraryManifest "${1:-.}"
    fi
fi
