# Orpheus

Orpheus is a small, lightweight text editor built using C and the ncurses library. It is designed
for simplicity and efficiency in the terminal environment.

## Features

* **Lightweight:** Minimal overhead for fast startup and operation.  
* **Ncurses UI:** Native terminal integration.  
* **Configurable:** Supports custom settings via a orpheus.config file.  
* **Smart Editing:** Features include auto-indentation and gap-buffer based text manipulation.

## Prerequisites

To compile and run Orpheus, you need the following:

* gcc (C17 standard)  
* ncurses development library (e.g., libncurses5-dev or ncurses-devel)  
* make

## Building and Installation

Use the provided Makefile to build, install, or clean the project:

### Build

To compile the project:

```Bash
make
```

### Install

To install the binary to your system (default is /usr/local/bin):

```Bash
sudo make install
```

### Uninstall

To remove the binary from your system:

```Bash
sudo make uninstall
```

### Clean

To remove built objects and the binary:

```Bash
make clean
```

## Usage

```Bash
orp [file file2 ...]
```

### **Keybindings**

| Command | Action |
| :---- | :---- |
| **Arrow keys** | Navigation |
| **PgUp / PgDn** | Page scroll |
| **Home / End** | Go to start/end of line |
| **Ctrl-S** | Save current file |
| **Ctrl-Q** | Quit (prompts if unsaved changes exist) |
| **Ctrl-F** | Find text (Enter to cycle, Esc to cancel) |
| **Ctrl-R** | Replace a set of characters with others |
| **Ctrl-G** | Go to specific line |
| **Ctrl-K** | Cut line |
| **Ctrl-U** | Paste (yank) line |
| **Ctrl-D** | Delete line |
| **Ctrl-A** | Go to start of line |
| **Ctrl-E** | Go to end of line |
| **Ctrl-W** | Toggle status bar visibility |
| **Ctrl-T** | Toggle TypeWriter mode |
| **Ctrl-O** | Open a new Buffer/tab |
| **Ctrl-P** | Go to previous tab |
| **Ctrl-N** | Go to next tab |

## Templates

```Bash
orp -t NAME [file file2 ...]
orp --template NAME [file file2 ...]
```

For every filename argument that does **not** already exist on disk, `-t`/`--template` populates
the new buffer with `~/.config/Orpheus/templates/NAME.tmpl` instead of leaving it empty. Files
that already exist are always opened as-is - the template is never applied to them. If no
filename is given at all, the template is applied to the new unnamed buffer instead.

Templates are plain text files and may use the placeholder `{{currentTime}}`, which is expanded
via `strftime()` using the [`time_format`](docs/CONFIGURATION.md) setting. For example, given
`~/.config/Orpheus/templates/chapter.tmpl`:

```Text
-----
Chapter 1
Draft 1
First Time: {{currentTime}}
Last Update: {{currentTime}}
-----
```

Running:

```Bash
orp -t chapter draft.txt
```

populates `draft.txt` with the template above (assuming `draft.txt` doesn't already exist),
expanding both `{{currentTime}}` placeholders to the current date/time.

## Configuration

You can customize Orpheus by creating a `~/.config/Orpheus/orpheus.config` file. The format uses
`setting=value`. Comments start with `#`.

Example:

```config
# Set tab width to 4 spaces
tab_width=4
```

Colours are fully customizable too, either by choosing a built-in `color_scheme` or by overriding
individual UI colours (text, status bar, line numbers, search highlight, and more).

Generate a Default Config by

```bash
orp --gen-config [file]
# If file not provided then generates file at
~/.config/Orpheus/orpheus.config
```

See [`docs/orpheus_config.example`](docs/orpheus_config.example) for an example file, and
**[docs/CONFIGURATION.md](docs/CONFIGURATION.md)** for the full settings reference, including all
colour customization options.

## Documentation

View Docs by running

```Bash
./scripts/docs.sh
```

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0). See the
[LICENSE](LICENSE) file for details.
