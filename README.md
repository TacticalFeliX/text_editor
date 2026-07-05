# tEdit - A Lightweight Terminal Text Editor in C

> This is a text editor written in pure C, and is built only for learning purposes.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Project Overview](#project-overview)
3. [Project Overview](#project-overview)
4. [Project Overview](#project-overview)
5. [Project Overview](#project-overview)
6. [Project Overview](#project-overview)
7. [Project Overview](#project-overview)
8. [Project Overview](#project-overview)
9. [Project Overview](#project-overview)
10. [Project Overview](#project-overview)
11. [Project Overview](#project-overview)
12. [Project Overview](#project-overview)

---

## Project Overview

**tEdit** is a lightweight, offline, terminal-based text editor written in C.
It is designed with two goals:

1. **Be a usable editor** - fast startup, small memory footprint, works without a GUI or network.
2. **Be a complete learning project** - test understanding of program architecture, buffer states, and understanding how editors work at a low level.

---

## Features

### File Operations
- New File
- Open file by path
- Save (`Ctrl+S`)
- Save As 
- Exit with unsaved-change warning

### Text Editing
- Insert characters
- Delete (forward and backward)
- Multi-line editing
- Line splitting (Enter)
- Line merging (Backspace at start of line)

### Navigation
- Arrow keys (Up, Down, Left, Right)
- Home / End (beginning and end of line)
- Page Up / Page Down
- Go to line (`Ctrl+G`)

### Status Bar
- Current filename
- Modified indicator (`[+]` when unsaved changes exist)
- Current line and column number
- Total line count

### Smart Editing
- **Auto bracket completion**: `(` → `()`, `[` → `[]`, `{` → `{}`, `"` → `""`, `'` → `''`
- **Smart indentation**: preserves and increases indent level after `{`, `if`, `for`, `while`
- **Configurable tab size**: 2, 4, or 8 spaces (default: 4)

### Undo / Redo
- Full undo history (`Ctrl+Z`)
- Full redo history (`Ctrl+Y`)
- Handles inserts, deletes, and multi-line operations

### Search
- Find (`Ctrl+F`)
- Find Next (`Ctrl+N`)
- Find Previous (`Ctrl+P`)
- Optional replace (`Ctrl+R`)

--- 

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                      main.c                             │
│         Entry point. Initialises all subsystems.        │
└──────────────────────┬──────────────────────────────────┘
                       │
         ┌─────────────▼──────────────┐
         │         editor.c           │
         │  Central coordinator.      │
         │  Owns the event loop.      │
         │  Dispatches key events.    │
         └──┬──────┬──────┬──────┬────┘
            │      │      │      │
    ┌───────▼──┐ ┌─▼────┐ ┌▼────▼──┐ ┌───────┐
    │terminal.c│ │buf.c │ │fileio.c│ │undo.c │
    │          │ │      │ │        │ │       │
    │Raw mode  │ │Array │ │open    │ │stack  │
    │Key decode│ │of    │ │save    │ │based  │
    │Screen    │ │lines │ │save as │ │undo   │
    │rendering │ │      │ │        │ │redo   │
    └──────────┘ └──────┘ └────────┘ └───┬───┘
                                         │
                                   ┌─────▼──────┐
                                   │  search.c  │
                                   │            │
                                   │ find/next/ │
                                   │ previous   │
                                   └────────────┘
                                   
```
Data flows in one direction: **key event → editor → buffer mutation → screen redraw**.

---

## Quick Start
```bash

cd text-editor
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG -o tedit src/*.c
./tedit <file path (optional)>
```

---

## Dependencies
tEdit has no build system and only dependencies include C99 compiler and the POSIX headers, everything compiles with a single `gcc` compiler

---

## Run Instructions

```bash
# Open a new empty buffer
./tedit

# Open an existing file
./tedit myfile.c

# Open with a specific tab size
./tedit --tabsize 2 myfile.c
```

---

## Usage Guide

When you open cEdit, you will see:

- A **text area** occupying most of the terminal
- A **title bar** at the top showing the filename
- A **status bar** at the bottom showing position and file state

Start typing immediately. The editor is always in insert mode (there are no modal states like Vim).

**To open a file from inside the editor:** `Ctrl+O`
**To save:** `Ctrl+S`
**To quit:** `Ctrl+Q` (you will be warned if there are unsaved changes)

---

## Keyboard Shortcuts

| Key             | Action                          |
|-----------------|---------------------------------|
| `Ctrl+S`        | Save                            |
| `Ctrl+O`        | Open file                       |
| `Ctrl+Q`        | Quit                            |
| `Ctrl+Z`        | Undo                            |
| `Ctrl+Y`        | Redo                            |
| `Ctrl+F`        | Find                            |
| `Ctrl+N`        | Find next                       |
| `Ctrl+P`        | Find previous                   |
| `Ctrl+G`        | Go to line                      |
| `Ctrl+R`        | Replace                         |
| `Home`          | Move to start of line           |
| `End`           | Move to end of line             |
| `Page Up`       | Scroll up one screen            |
| `Page Down`     | Scroll down one screen          |
| `Arrow keys`    | Move cursor                     |
| `Enter`         | New line (with smart indent)    |
| `Tab`           | Insert spaces (configurable)    |
| `Backspace`     | Delete character before cursor  |
| `Delete`        | Delete character after cursor   |

---

## Project Structure

```
text-editor/
│
├── src/                    # All C source and header files
│   ├── main.c              # Entry point
│   ├── editor.c / .h       # Central editor state and event loop
│   ├── buffer.c / .h       # Text buffer (array of lines)
│   ├── terminal.c / .h     # Raw mode, rendering, key decode
│   ├── fileio.c / .h       # File open, save, save as
│   ├── undo.c / .h         # Undo/redo stack
│   └── search.c / .h       # Find / replace
│
├── docs/                   # Design and learning documentation
│   ├── architecture.md     # Full system architecture
│   ├── editor-design.md    # Editor state machine design
│   ├── buffer-design.md    # Buffer data structure analysis
│   ├── terminal-programming.md  # How terminals work
│   └── undo-redo.md        # Undo/redo system design
│
├── tests/                  # Unit tests
│   ├── test_buffer.c
│   ├── test_undo.c
│   └── test_search.c
│
├── examples/               # Example files to open and test
│
└── README.md               # This file
```


