# Configuration

You can customize Orpheus by creating a `~/.config/Orpheus/orpheus.config` file. The format uses
`setting=value`, one per line. Comments start with `#`.

Example:

```config
# Set tab width to 4 spaces
tab_width=4
```

See [`orpheus.config.example`](../orpheus.config.example) for a full example file.

If the config file does not exist, every setting falls back to its compiled-in default and Orpheus
starts normally. Unrecognised keys are silently ignored.

## Full Settings Reference

* **tab_width** (int): Sets the number of spaces to tab over.

* **show_line_numbers** (int): If 0, hides the line-number gutter. Any other value shows it.
  Default: 1.

* **auto_indent** (int): If non-zero, pressing Enter copies the leading whitespace of the current
  line to the new line automatically. Default: 1.

* **show_statusbar** (int): If 0, hides the bottom status/command bar, giving one extra row of
  text. Default: 1.

* **cursor_style** (int): Controls the terminal cursor shape passed to `curs_set()`.
  0 = invisible, 1 = normal (default), 2 = very visible / block.

* **color_scheme** (string): Built-in colour theme for the UI chrome.
  * `default` - system default colours (no change)
  * `dark` - white text on dark backgrounds
  * `light` - black text on light backgrounds
  * `mono` - disables colour entirely (`A_REVERSE` for highlights)

* **gutter_width** (int): The number of spaces for the width of the line number gutter.
  Default: 5.

* **key_delay** (int): The time it takes to wait for escape-sequence processing. Default: 50.

* **focus_mode** (int): 0 = normal, 1 = focus/typewriter mode (Ctrl-T). Default: 0.

* **focus_width** (int): Text column width in focus mode. Default: 72.

* **time_format** (string): `strftime()` format string used to expand `{{currentTime}}` in
  templates. Default: `"%-m/%-d/%y"` (e.g. `"6/25/26"`). Note: `%-m`/`%-d` are glibc extensions
  (no leading zero); on non-glibc systems (e.g. macOS/BSD libc) use `%m`/`%d` instead if the
  output looks wrong (zero-padded or literal).

## Custom Colours

Beyond the built-in `color_scheme` palettes, you can override the foreground and/or background
colour of any individual UI element. An override always takes priority over whatever
`color_scheme` would otherwise assign, so you can start from a built-in scheme and tweak just the
pieces you don't like - for example, keeping `dark` but giving search highlights a colour of
your own:

```config
color_scheme=dark
color_search_bg=magenta
```

Or omit `color_scheme` entirely (or set it to `default`) and build a palette from scratch using
only overrides.

### Recognised colour names

Values are case-insensitive: `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`,
`white`, `default` (the terminal's own colour for that slot).

### Available keys

Each UI element has a `_fg` (foreground/text) and `_bg` (background) key. All twelve are optional;
any key you don't set keeps whatever `color_scheme` assigns.

| Key | UI element |
| :---- | :---- |
| `color_normal_fg` / `color_normal_bg` | Main text area |
| `color_status_fg` / `color_status_bg` | Status bar (filename, cursor position, word/char count) |
| `color_cmdbar_fg` / `color_cmdbar_bg` | Command bar (keybinding hints) |
| `color_lnum_fg` / `color_lnum_bg` | Line-number gutter |
| `color_search_fg` / `color_search_bg` | Search match highlight |
| `color_select_fg` / `color_select_bg` | Mouse selection highlight |

### Example: full custom palette

```config
color_scheme=default
color_normal_fg=white
color_normal_bg=black
color_status_fg=black
color_status_bg=cyan
color_cmdbar_fg=black
color_cmdbar_bg=cyan
color_lnum_fg=green
color_lnum_bg=black
color_search_fg=black
color_search_bg=yellow
color_select_fg=black
color_select_bg=white
```

Colour settings have no effect if `color_scheme=mono`, since colour support is disabled entirely
in that mode.
