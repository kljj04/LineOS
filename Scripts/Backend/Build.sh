#!/usr/bin/env bash
# Scripts/Backend/Build.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

BuildDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BuildRoot="$(cd -- "$BuildDir/../.." && pwd)"

BuildProgressWrite()
{
    if [[ -z "${RunProgressFile:-}" ]]; then
        return 0
    fi

    printf '%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" > "$RunProgressFile.next" &&
        mv -f -- "$RunProgressFile.next" "$RunProgressFile"
}

BuildExecute()
{
    local Target="$1"
    local Plan Line File Status
    local Total=0
    local Current=0
    local Completed=0
    local Make=(make -C "$BuildRoot" --no-print-directory --silent -f Makefiles/Makefile -j$(nproc))

    if [[ -n "${BuildProgram:-}" ]]; then
        Make+=("Program=$BuildProgram")
    fi

    if [[ ! -f "$BuildRoot/Makefiles/Makefile" ]]; then
        return 1
    fi

    case "$Target" in
        All|UserProgram)
            if ! Plan=$("${Make[@]}" -n "$Target"); then
                return 1
            fi

            while IFS= read -r Line; do
                if [[ "$Line" == "printf 'LOS_PROGRESS"* ]]; then
                    Total=$((Total + 1))
                fi
            done <<< "$Plan"
            ;;
        Format|Clean)
            if ! Plan=$("${Make[@]}" "Progress${Target}List"); then
                return 1
            fi

            while IFS= read -r Line; do
                if [[ -n "$Line" ]]; then
                    Total=$((Total + 1))
                fi
            done <<< "$Plan"
            ;;
    esac

    if (( Total == 0 )); then
        case "$Target" in
            All|UserProgram)
                File="Up to date"
                ;;
            Clean)
                File="Nothing to clean"
                ;;
            Format)
                File="No files to format"
                ;;
        esac
    else
        File="Waiting"
    fi

    BuildProgressWrite "$Total" 0 0 "$File"
    "${Make[@]}" "$Target" | while IFS= read -r Line; do
        case "$Line" in
            LOS_PROGRESS$'\t'*)
                File=${Line#*$'\t'}
                Current=$((Current + 1))
                BuildProgressWrite "$Total" "$Current" "$Completed" "$File"
                printf '[%s/%s] %s\n' "$Total" "$Current" "$File"
                ;;
            LOS_DONE$'\t'*)
                Completed=$((Completed + 1))
                BuildProgressWrite "$Total" "$Current" "$Completed" "${Line#*$'\t'}"
                ;;
            *)
                printf '%s\n' "$Line"
                ;;
        esac
    done

    Status=${PIPESTATUS[0]}

    return "$Status"
}

BuildRun()
{
    if [[ -n "${BuildProgram:-}" ]]; then
        BuildExecute UserProgram
    else
        BuildExecute All
    fi
}

BuildClean()
{
    BuildExecute Clean
}

BuildFormat()
{
    BuildExecute Format
}
