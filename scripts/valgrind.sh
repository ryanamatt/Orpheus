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

set -euo pipefail

EXECUTABLE="./orp"
LOG_FILE="valgrind-check.txt"
LEAK_CHECK="full"

show_help() {
    echo "Usage: $0 [options] [executable]"
    echo ""
    echo "Options:"
    echo "  -l, --leak-check [type]  Set leak check level (none, summary, full). Default: full"
    echo "  -o, --output [file]      Specify output log file. Default: valgrind-check.txt"
    echo "  -h, --help               Show this help message"
    echo ""
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -l|--leak-check) LEAK_CHECK="$2"; shift ;;
        -o|--output) LOG_FILE="$2"; shift ;;
        -h|--help) show_help; exit 0 ;;
        #) EXECUTABLE="$1" ;;
    esac
    shift
done

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable '$EXECUTABLE' not found."
    exit 1
fi

echo "Running Valgrind on: $EXECUTABLE"
echo "Log file: $LOG_FILE"

# Run Valgrind
valgrind --leak-check="$LEAK_CHECK" \
         --track-origins=yes \
         --log-file="$LOG_FILE" \
         "$EXECUTABLE"

echo "Analysis complete. Results saved to $LOG_FILE"