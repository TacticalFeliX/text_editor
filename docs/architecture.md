# tEdit Architecture

> This document describes the complete technical architecture of tEdit.
> Read this before reading any source code. Everything here is intentional.

---

## Table of Contents

1. [Design Philosophy](#design-philosophy)
2. [Component Map](#component-map)
3. [Module Dependency Graph](#module-dependency-graph)
4. [Data Flow: Key Press to Screen](#data-flow-key-press-to-screen)
5. [Memory Layout](#memory-layout)
6. [Rendering Flow](#rendering-flow)
7. [Event Loop Design](#event-loop-design)
8. [Why No ncurses?](#why-no-ncurses)
9. [Error Handling Strategy](#error-handling-strategy)
10. [Platform Abstraction](#platform-abstraction)

---

## Design Philosophy

tEdit is built around three architectural principles:

### 1. Single Direction Data Flow

Every interaction follows this path:

```
User Input → Editor → Buffer Mutation → Screen Redraw
```

The screen is **never** modified directly by user input. Input causes a mutation,
and mutation causes a redraw. This makes the system predictable and debuggable:
to understand any screen state, you only need to know the buffer contents.

### 2. Module Independence

Each module (`buffer`, `terminal`, `fileio`, `undo`, `search`) knows nothing
about the others. They expose a public API. Only `editor.c` coordinates between
them.

This is not a bureaucratic rule. It has a practical consequence: you can test
any module in isolation. `tests/test_buffer.c` runs without a terminal, without
files, and without an undo stack. The module works on its own.

### 3. Explicit Is Better Than Clever

Every decision in the code is the simplest decision that works correctly.
Where a clever solution was available, the simple one was chosen and the clever
one is documented as a "future improvement."

---

## Component Map

```
╔══════════════════════════════════════════════════════════════════════╗
║                            tEdit process                             ║
║                                                                      ║
║  ┌────────────────────────────────────────────────────────────────┐  ║
║  │                          main.c                                │  ║
║  │                                                                │  ║
║  │  • Parses command-line arguments                               │  ║
║  │  • Initialises all subsystems                                  │  ║
║  │  • Passes control to editor_run()                              │  ║
║  │  • Cleans up and exits                                         │  ║
║  └──────────────────────────┬─────────────────────────────────────┘  ║
║                             │ calls                                  ║
║  ┌──────────────────────────▼─────────────────────────────────────┐  ║
║  │                        editor.c                                │  ║
║  │                                                                │  ║
║  │  • Owns the EditorState struct (cursor pos, viewport, mode)    │  ║
║  │  • Runs the event loop: read key → dispatch → redraw           │  ║
║  │  • Calls buffer_* for all text mutations                       │  ║
║  │  • Calls terminal_* for all screen writes                      │  ║
║  │  • Calls undo_* before every mutation                          │  ║
║  │  • Calls search_* on Ctrl+F                                    │  ║
║  │  • Calls fileio_* on Ctrl+S, Ctrl+O                            │  ║
║  └──┬──────────┬──────────┬──────────┬──────────┬─────────────────┘  ║
║     │          │          │          │          │                    ║
║  ┌──▼──────┐ ┌─▼───────┐ ┌▼───────┐ ┌▼──────┐ ┌▼──────────┐          ║
║  │terminal │ │buffer   │ │fileio  │ │undo   │ │search     │          ║
║  │         │ │         │ │        │ │       │ │           │          ║
║  │raw mode │ │lines[]  │ │fopen   │ │stack  │ │substring  │          ║
║  │key read │ │insert   │ │fread   │ │push   │ │find       │          ║
║  │render   │ │delete   │ │fwrite  │ │pop    │ │find_next  │          ║
║  │cursor   │ │append   │ │reload  │ │redo   │ │replace    │          ║
║  └─────────┘ └─────────┘ └────────┘ └───────┘ └───────────┘          ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Module Dependency Graph

Arrows mean "depends on" (imports the header of):

```
main.c ──────────────────► editor.h

editor.c ────────────────► buffer.h
         ├───────────────► terminal.h
         ├───────────────► fileio.h
         ├───────────────► undo.h
         └───────────────► search.h

fileio.c ────────────────► buffer.h   (reads into / writes from buffer)

undo.c ──────────────────► buffer.h   (stores buffer snapshots)

search.c ────────────────► buffer.h   (searches inside buffer)

terminal.c ──────────────► (no project dependencies — only <termios.h>)

buffer.c ────────────────► (no project dependencies — only <stdlib.h>)
```

**Critical rule:** No module may import another module except via `editor.c`.
`buffer.c` must not import `undo.h`. `search.c` must not import `terminal.h`.
This rule keeps the graph acyclic and modules testable in isolation.

The only exception is that `fileio.c`, `undo.c`, and `search.c` all import
`buffer.h` — they need the `Buffer` type. This is intentional. They operate
*on* the buffer but do not coordinate with each other.

---

## Data Flow: Key Press to Screen

This is the most important flow to understand. Every editing operation follows
this exact sequence.

### Example: User presses the letter 'A'

```
STEP 1: Terminal delivers raw bytes
──────────────────────────────────
  The terminal is in raw mode. When the user presses 'A', the kernel
  delivers the byte 0x41 directly to our read() call.
  No buffering. No line discipline. No echo.

  Read in terminal.c:
    terminal_read_key() → returns KEY_CHAR with value 'A'

STEP 2: Editor dispatches the event
────────────────────────────────────
  The event loop in editor.c receives KeyEvent { type=KEY_CHAR, ch='A' }.
  It calls: editor_handle_char(state, 'A')

STEP 3: Undo record is pushed BEFORE mutation
──────────────────────────────────────────────
  Before changing the buffer, we save what we are about to do:
    undo_push(state->undo, UNDO_INSERT, row, col, 'A')

  The undo stack now knows: "if we undo, delete the char at (row, col)"

STEP 4: Buffer mutation
────────────────────────
  buffer_insert_char(state->buf, row, col, 'A')
  
  Inside buffer.c:
  - The line at index `row` is extended by 1 byte (realloc)
  - All characters from col onward are shifted right by 1
  - The new character 'A' is placed at position col
  - buf->modified = 1

STEP 5: Cursor advances
────────────────────────
  state->cursor_col += 1
  (Handled in editor.c after the buffer call)

STEP 6: Screen redraw
─────────────────────
  editor_refresh_screen(state) is called.
  
  Inside terminal.c:
  - Cursor is hidden (prevents flicker)
  - All visible lines are redrawn from the buffer
  - Status bar is redrawn
  - Cursor is repositioned
  - Cursor is shown again

  The screen now shows the 'A' character.

Total time: <1ms on any modern machine.
```

### Example: User presses Ctrl+Z (Undo)

```
STEP 1: terminal_read_key() returns KEY_CTRL('Z')

STEP 2: editor_handle_key() dispatches to editor_do_undo()

STEP 3: undo_pop(state->undo) retrieves the top record
  Record says: UNDO_INSERT at (row=5, col=12), char='A'
  "We inserted 'A' at (5,12) — to undo, delete it."

STEP 4: buffer_delete_char(state->buf, 5, 12)
  Removes the character. Buffer returns to previous state.

STEP 5: Cursor moves back
  state->cursor_col -= 1

STEP 6: Screen redraw
  Terminal redraws everything from the buffer.
```

---

## Memory Layout

At runtime, a tEdit process holds the following in memory:

```
 Stack                          Heap
 ─────                          ────

 main() frame
   └─ EditorState state
       ├─ Buffer *buf ──────────► Buffer struct
       │                            ├─ char **lines ──► [ ptr0, ptr1, ptr2, ... ]
       │                            │                       │     │     │
       │                            │                       ▼     ▼     ▼
       │                            │                    "int " "main" "{\n"
       │                            ├─ int *line_lengths ► [ 4, 4, 2, ... ]
       │                            ├─ int line_count = 7
       │                            ├─ int line_capacity = 64
       │                            └─ int modified = 1
       │
       ├─ UndoStack *undo ──────► UndoStack struct
       │                            ├─ UndoNode *top ──► UndoNode
       │                            │                      ├─ type = UNDO_INSERT
       │                            │                      ├─ row = 5
       │                            │                      ├─ col = 12
       │                            │                      ├─ ch = 'A'
       │                            │                      └─ *prev ──► UndoNode → ...
       │                            └─ int depth = 42
       │
       ├─ SearchCtx *search ────► SearchCtx struct
       │                            ├─ char query[256]
       │                            ├─ int last_row
       │                            └─ int last_col
       │
       ├─ int cursor_row = 5
       ├─ int cursor_col = 12
       ├─ int scroll_row = 0
       ├─ int term_rows = 24
       ├─ int term_cols = 80
       └─ char filename[256]
```

**Key observations:**

1. The `Buffer` struct owns all text memory. It is the single source of truth.
2. The `UndoStack` is a linked list of nodes. Each node is a small struct.
   For character-level operations, each node is ~24 bytes.
3. The terminal dimensions are stored in `EditorState` and refreshed on
   `SIGWINCH` (terminal resize signal).
4. No global variables. Everything is threaded through `EditorState`.

---

## Rendering Flow

The terminal screen is redrawn from scratch on every key press. This sounds
expensive but is not — the amount of text visible in a terminal (typically
24×80 = 1920 characters) is trivially small compared to what modern CPUs
can process in a millisecond.

```
editor_refresh_screen(EditorState *state)
    │
    ├─ 1. Hide cursor
    │      write("\x1b[?25l")
    │
    ├─ 2. Move cursor to top-left
    │      write("\x1b[H")
    │
    ├─ 3. For each visible row (scroll_row to scroll_row + term_rows - 2):
    │      │
    │      ├─ 3a. Clear the line
    │      │       write("\x1b[K")
    │      │
    │      └─ 3b. Write the visible portion of the buffer line
    │              (from column scroll_col, up to term_cols characters)
    │              buffer_get_line(buf, row)  →  write to stdout
    │
    ├─ 4. Draw status bar (last row)
    │      write filename, modified flag, line/col, total lines
    │
    ├─ 5. Position cursor
    │      write("\x1b[%d;%dH", cursor_row - scroll_row + 1,
    │                           cursor_col - scroll_col + 1)
    │
    └─ 6. Show cursor
           write("\x1b[?25h")
```

The entire render is assembled in a **write buffer** (a `char` array in
`terminal.c`) and sent to `stdout` with a single `write()` call. This avoids
the flickering caused by many small writes.

---

## Event Loop Design

```c
/* Simplified pseudocode of editor_run() in editor.c */

void editor_run(EditorState *state)
{
    /* Draw the initial screen before waiting for input */
    editor_refresh_screen(state);

    while (state->running)
    {
        /* Block until a key is available.
           This is the ONLY place the process sleeps. */
        KeyEvent key = terminal_read_key();

        /* Dispatch to the appropriate handler.
           No handler directly touches the terminal — they only
           mutate state and return. */
        editor_handle_key(state, key);

        /* Redraw. Always. Every key press.
           We do not try to be clever about partial redraws in v1. */
        editor_refresh_screen(state);
    }
}
```

The loop is intentionally simple. There is no event queue, no threading, no
timers. Every key press is handled synchronously. This makes the control flow
linear and easy to debug.

**Why no threading?** Terminal editors do not need threads in v1. The render
is fast enough. The only future case for a thread would be an LSP server
communication (v5) where we need to read server responses without blocking
the edit loop. That is a v5 problem.

---

## Why No ncurses?

ncurses is a widely-used library for terminal applications. Here is why we
chose not to use it:

### What ncurses provides

- Terminal capability database (terminfo/termcap): handles hundreds of different
  terminal types
- A windowing model with `WINDOW` objects
- Line drawing characters, colour pairs
- Automatic optimised screen updates (only redraw changed cells)

### Why we don't need it

1. **Terminal compatibility:** In 2024, virtually every terminal supports ANSI
   escape sequences (the VT100 standard from 1978). The edge cases ncurses
   handles (e.g., HP terminals from the 1980s) are not our concern.

2. **Learning value:** If we use ncurses, we never understand *how* terminals
   work. The entire point of this project is to understand the layer that
   ncurses abstracts.

3. **Dependency cost:** ncurses must be installed separately on some systems.
   Our project has zero dependencies, so `gcc main.c ... -o tEdit` works
   anywhere.

4. **Complexity:** ncurses introduces a large API surface. Understanding
   tEdit's `terminal.c` (one file, ~400 lines) is faster than understanding
   the ncurses documentation.

### When ncurses IS the right choice

If you need:
- Compatibility with dozens of historical terminal types
- Complex overlapping windows
- Box-drawing characters
- Color pairs beyond basic ANSI

...then ncurses is excellent. For tEdit, none of these apply.

---

## Error Handling Strategy

tEdit uses a two-tier error handling approach:

### Tier 1: Unrecoverable errors (startup and allocation failures)

These call `fprintf(stderr, ...)` and `exit(EXIT_FAILURE)`.

Examples:
- `malloc` returns NULL during buffer initialisation
- The terminal does not support raw mode
- A required file cannot be opened

These are rare and occur at startup where no user state exists to lose.

### Tier 2: Recoverable errors (user operations)

These return error codes (`0` for success, `-1` for failure) and set a message
in the status bar.

Examples:
- File save fails (disk full)
- File open fails (permission denied)
- Search finds no match

The status bar displays the error message for one keypress, then reverts to
normal display.

**There are no exceptions, no `setjmp`/`longjmp`, and no global error state.**
Error information is returned through return values and the EditorState's
`status_message` field.

---

## Platform Abstraction

tEdit targets Linux primarily and Windows (MinGW) secondarily.

The platform-specific code is confined to `terminal.c`:

| Feature              | Linux (POSIX)              | Windows (MinGW)              |
|----------------------|----------------------------|------------------------------|
| Raw mode             | `tcsetattr(TCSAFLUSH)`     | `SetConsoleMode()`           |
| Terminal size        | `ioctl(TIOCGWINSZ)`        | `GetConsoleScreenBufferInfo` |
| Read key             | `read(STDIN_FILENO)`       | `_getch()` or `ReadConsoleInput` |
| ANSI escape support  | Native in all terminals    | Enabled via `ENABLE_VIRTUAL_TERMINAL_PROCESSING` |

The `terminal.h` public API is identical on both platforms. The `#ifdef`
guards live inside `terminal.c`, hidden from all other modules.

`buffer.c`, `fileio.c`, `undo.c`, and `search.c` are **pure C** with no
platform-specific code at all. They work identically on Linux and Windows.
