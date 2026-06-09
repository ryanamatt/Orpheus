#!/usr/bin/env bash
#
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

# scripts/docs.sh
#
# Generate Doxygen documentation from the project Doxyfile and open
# the result in the default browser.
#
# Install: sudo apt install doxygen  /  sudo pacman -S doxygen
#
# Usage:
#   ./scripts/docs.sh          # generate and open
#   ./scripts/docs.sh --no-open  # generate only (useful in CI)

set -euo pipefail

OPEN=1
for arg in "$@"; do
    [[ "$arg" == "--no-open" ]] && OPEN=0
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DOXYFILE="${REPO_ROOT}/Doxyfile"
# Read OUTPUT_DIRECTORY from Doxyfile;=. fall back to docs/html
OUTPUT_DIR=$(grep -E "^OUTPUT_DIRECTORY\s*=" "${DOXYFILE}" 2>/dev/null \
    | awk -F'=' '{print $2}' | xargs)
OUTPUT_DIR="${OUTPUT_DIR:-docs}"
INDEX="${REPO_ROOT}/${OUTPUT_DIR}/html/index.html"

command -v doxygen &>/dev/null || {
    echo "error: doxygen not found. Install it and re-run." >&2
    exit 1
}

[[ -f "${DOXYFILE}" ]] || {
    echo "error: Doxyfile not found at ${DOXYFILE}" >&2
    exit 1
}

echo "==> Running Doxygen"
cd "${REPO_ROOT}"
doxygen "${DOXYFILE}"
echo "==> Output: ${INDEX}"

if [[ $OPEN -eq 1 ]]; then
    xdg-open "${INDEX}" 2>/dev/null \
        || open "${INDEX}" 2>/dev/null \
        || echo "      Open ${INDEX} manually."
fi