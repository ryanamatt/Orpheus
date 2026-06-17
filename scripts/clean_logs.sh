#!/usr/bin/env bash

# Orpheus - a small ncurses text editor
#
# Copyright (C) 2026 Ryan Mattson
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# scripts/clean_logs.sh
#
# Delete log files from logs/ that are older than KEEP_DAYS days.
# Defaults to 7 days; pass a number as the first argument to override.
#
# Usage:
#   ./scripts/clean_logs.sh        # delete logs older than 7 days
#   ./scripts/clean_logs.sh 3      # delete logs older than 3 days
#   ./scripts/clean_logs.sh 0      # delete all logs

set -euo pipefail

KEEP_DAYS="${1:-7}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="${REPO_ROOT}/logs"

if [[ ! -d "${LOG_DIR}" ]]; then
    echo "logs/ directory not found — nothing to do."
    exit 0
fi

# Count before
total_before=$(find "${LOG_DIR}" -maxdepth 1 -name "*.log" | wc -l)

if [[ $total_before -eq 0 ]]; then
    echo "logs/ is already empty."
    exit 0
fi

echo "==> Removing logs older than ${KEEP_DAYS} day(s) from logs/"

if [[ "${KEEP_DAYS}" -eq 0 ]]; then
    # Remove everything
    find "${LOG_DIR}" -maxdepth 1 -name "*.log" -delete
else
    find "${LOG_DIR}" -maxdepth 1 -name "*.log" -mtime "+${KEEP_DAYS}" -delete
fi

total_after=$(find "${LOG_DIR}" -maxdepth 1 -name "*.log" | wc -l)
removed=$(( total_before - total_after ))

echo "    Removed : ${removed} file(s)"
echo "    Kept    : ${total_after} file(s)"