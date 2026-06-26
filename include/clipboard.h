/*
 * Orpheus - a small ncurses text editor
 *
 * Copyright (C) 2026 Ryan Mattson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

/**
 * @brief Probe the host for a usable system-clipboard tool.
 *
 * Checks, in order, for wl-copy/wl-paste (Wayland), xclip or xsel (X11),
 * pbcopy/pbpaste (macOS), and clip.exe/powershell.exe (WSL). The result is
 * cached after the first call, so this is cheap to call repeatedly.
 *
 * @return 1 if a backend is available, 0 if Orpheus should fall back to its
 *         internal clipboard only.
 */
int clipboard_available(void);

/**
 * @brief Copy @p text to the system clipboard.
 *
 * No-op (returns 0) if no backend was detected by clipboard_available().
 *
 * @param text   Buffer holding the bytes to copy.
 * @param len    Number of bytes in @p text (no terminator required).
 * @return 1 on success, 0 on failure or if no backend is available.
 */
int clipboard_set(const char *text, int len);

/**
 * @brief Read the current system clipboard contents.
 *
 * No-op (returns 0) if no backend was detected by clipboard_available().
 *
 * @param out     Destination buffer.
 * @param outsize Capacity of @p out, including space for the terminator.
 * @return Number of bytes written to @p out (excluding terminator), or -1
 *         on failure / no backend available. @p out is always
 *         NUL-terminated on success.
 */
int clipboard_get(char *out, int outsize);

#endif // CLIPBOARD_H