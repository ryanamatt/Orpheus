# Orpheus

Orpheus is a small, lightweight text editor built using C and the ncurses library. It is designed
for simplicity and efficiency in the terminal environment.

## Features

* **Lightweight:** Minimal overhead for fast startup and operation.  
* **Ncurses UI:** Native terminal integration.  
* **Configurable:** Supports custom settings via a .orpheusrc file.  
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

## Configuration

You can customize Orpheus by creating a ~/.config/.orpheusrc file. The format uses setting=value. Comments start with \#.

Example:

```.orpheusrc
# Set tab width to 4 spaces  
tab_width=4
```

See .orpeheusrc.example for an example file.

Full Conguration Settings:

* tab_width (int): Sets the number of spaces to tab over
* show_line_numbers (int): If 0, hides the line-number gutter. Any other value shows it. Default: 1
* auto_indent (int): If non-zero, pressing Enter copies the leading whitespace of the current line
to the new line automatically. Default: 1.

* show_statusbar (int): If 0, hides the bottom status/command bar, giving one extra row of text
Default: 1.

* cursor_style (int): Controls the terminal cursor shape passed to curs_set() 0 = invisible, 1 =
normal (default), 2 = very visible / block.

* color_scheme (string): Built-in colour theme for the UI chrome.f
  * default  - system default colours (no change)
  * dark     - white text on dark backgrounds
  * light    - black text on light backgrounds
  * mono     - disables colour entirely (A_REVERSE for highlights)

* gutter_width (int):The number of spaces for the width of the line number gutter. Default 5.

* key_delay (int): The time it takes to wait for escape-sequence processing. Default 50.

* focus_mode (int): 0=normal, 1=focus/typewriter mode (Ctrl-T). Default: 0

* focus_width (int) : Text column width in focus mode. Default: 72

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0). See the
[LICENSE](LICENSE) file for details.
