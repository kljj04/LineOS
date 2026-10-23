#!/usr/bin/env bash
# Scripts/Run.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

set -Eeuo pipefail

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

echo ""

exec node "$ScriptDir/JS/Run.js" "$@"