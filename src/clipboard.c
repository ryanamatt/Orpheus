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

/* Expose popen/pclose (POSIX, not standard C) regardless of which -std=
 * flag the build uses. Must be defined before any system header is
 * included, which is why this comes before <stdio.h> below. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logger.h"
#include "clipboard.h"

/**
 * @brief Identifies which external tool pair is used to reach the system
 *        clipboard on the current host.
 */
typedef enum {
    CB_NONE,      /**< No backend found - internal clipboard only. */
    CB_WAYLAND,   /**< wl-copy / wl-paste (Wayland). */
    CB_XCLIP,     /**< xclip (X11). */
    CB_XSEL,      /**< xsel (X11). */
    CB_PBCOPY,    /**< pbcopy / pbpaste (macOS). */
    CB_WSL        /**< clip.exe / powershell.exe (WSL -> Windows clipboard). */
} ClipboardBackend;

static int detected = 0;
static ClipboardBackend backend = CB_NONE;

/**
 * @brief Run @c command -V via @c sh to check whether a binary is on PATH.
 *
 * @param command Name of the executable to look up.
 * @return 1 if found, 0 otherwise.
 */
static int tool_exists(const char *command) {
    char check[256];
    snprintf(check, sizeof(check), "command -v %s >/dev/null 2>&1", command);
    return system(check) == 0;
}

/**
 * @brief Detect which clipboard backend, if any, the host supports.
 *
 * Detection order:
 *   1. WSL (presence of /proc/sys/fs/binfmt_misc/WSLInterop or $WSL_DISTRO_NAME)
 *      -> clip.exe / powershell.exe, reaching the real Windows clipboard.
 *   2. Wayland (WAYLAND_DISPLAY set) -> wl-copy / wl-paste.
 *   3. X11 (DISPLAY set) -> xclip, then xsel.
 *   4. macOS -> pbcopy / pbpaste.
 *
 * Cached in @c backend after the first call so repeated cut/paste calls
 * don't re-probe the system every time.
 */
static void detect_backend(void) {
    if (detected) return;
    detected = 1;

    if (getenv("WSL_DISTRO_NAME") || access("/proc/sys/fs/binfmt_misc/WSLInterop", F_OK) == 0) {
        if (tool_exists("clip.exe") && tool_exists("powershell.exe")) {
            backend = CB_WSL;
            log_debug("clipboard: detected WSL backend (clip.exe / powershell.exe)");
            return;
        }
    }

    if (getenv("WAYLAND_DISPLAY") && tool_exists("wl-copy") && tool_exists("wl-paste")) {
        backend = CB_WAYLAND;
        log_debug("clipboard: detected Wayland backend (wl-copy / wl-paste)");
        return;
    }

    if (getenv("DISPLAY")) {
        if (tool_exists("xclip")) {
            backend = CB_XCLIP;
            log_debug("clipboard: detected X11 backend (xclip)");
            return;
        }
        if (tool_exists("xsel")) {
            backend = CB_XSEL;
            log_debug("clipboard: detected X11 backend (xsel)");
            return;
        }
    }

    if (tool_exists("pbcopy") && tool_exists("pbpaste")) {
        backend = CB_PBCOPY;
        log_debug("clipboard: detected macOS backend (pbcopy / pbpaste)");
        return;
    }

    backend = CB_NONE;
    log_debug("clipboard: no system clipboard backend found - using internal clipboard only");
}

int clipboard_available(void) {
    detect_backend();
    return backend != CB_NONE;
}

int clipboard_set(const char *text, int len) {
    detect_backend();

    const char *cmd = NULL;
    switch (backend) {
        case CB_WAYLAND: cmd = "wl-copy";                          break;
        case CB_XCLIP:    cmd = "xclip -selection clipboard -in";  break;
        case CB_XSEL:     cmd = "xsel --clipboard --input";        break;
        case CB_PBCOPY:   cmd = "pbcopy";                          break;
        case CB_WSL:      cmd = "clip.exe";                        break;
        case CB_NONE:     /* fall through */
        default:          return 0;
    }

    FILE *p = popen(cmd, "w");
    if (!p) {
        log_error("clipboard_set: failed to launch '%s'", cmd);
        return 0;
    }

    size_t written = fwrite(text, 1, (size_t)len, p);
    int ok = (pclose(p) == 0) && (written == (size_t)len);
    if (!ok) log_error("clipboard_set: '%s' did not complete successfully", cmd);
    return ok;
}

int clipboard_get(char *out, int outsize) {
    detect_backend();

    const char *cmd = NULL;
    switch (backend) {
        case CB_WAYLAND: cmd = "wl-paste --no-newline";                  break;
        case CB_XCLIP:    cmd = "xclip -selection clipboard -out";       break;
        case CB_XSEL:     cmd = "xsel --clipboard --output";             break;
        case CB_PBCOPY:   cmd = "pbpaste";                                break;
        // powershell Get-Clipboard adds a trailing CRLF; strip it below.
        case CB_WSL:      cmd = "powershell.exe -noprofile -command Get-Clipboard"; break;
        case CB_NONE:     /* fall through */
        default:          return -1;
    }

    FILE *p = popen(cmd, "r");
    if (!p) {
        log_error("clipboard_get: failed to launch '%s'", cmd);
        return -1;
    }

    int n = (int)fread(out, 1, (size_t)outsize - 1, p);
    pclose(p);

    if (n < 0) return -1;

    // Strip a single trailing CRLF/LF that some tools (notably PowerShell) append.
    if (n > 0 && out[n - 1] == '\n') n--;
    if (n > 0 && out[n - 1] == '\r') n--;

    out[n] = '\0';
    return n;
}