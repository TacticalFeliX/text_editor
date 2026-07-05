# Editor Design

> This document explains the design of the central editor module: the event loop,
> the EditorState struct, and the dispatch system that coordinates all subsystems.

---

## Table of Contents

1. [The Editor's Job](#the-editors-job)
2. [EditorState — The Central Struct](#editorstate--the-central-struct)
3. [The Event Loop](#the-event-loop)
4. [Key Dispatch Table](#key-dispatch-table)
5. [Cursor and Viewport Management](#cursor-and-viewport-management)
6. [Smart Editing Features](#smart-editing-features)
7. [Mode Design](#mode-design)
8. [Status Bar](#status-bar)

---

## The Editor's Job

`editor.c` is the conductor of the orchestra. It does not play any instrument
itself — it just tells other modules when to act.

Concretely:

- It **does not** manage text — that is `buffer.c`
- It **does not** draw to the screen — that is `terminal.c`
- It **does not** read or write files — that is `fileio.c`
- It **does not** manage undo records — that is `undo.c`
- It **does not** search text — that is `search.c`

What `editor.c` *does*:

1. Owns the `EditorState` (cursor position, viewport, filename, mode)
2. Runs the event loop
3. Receives key events and decides what action to take
4. Calls the appropriate subsystems in the right order
5. Manages the screen refresh cycle

---

## EditorState — The Central Struct

```c
/*
 * EditorState — the complete runtime state of a running tEdit instance.
 *
 * This struct is allocated once in main() and passed to every function
 * that needs to read or modify editor state. There are no global variables.
 * All state lives here.
 *
 * Having all state in one struct makes the system:
 *   - Testable: you can create a synthetic EditorState in tests
 *   - Debuggable: all state is visible in a single debugger print
 *   - Future-proof: adding state is just adding a field
 */
typedef struct EditorState
{
    /* ── Buffer ─────────────────────────────────────────────────── */
    Buffer     *buf;            /* The open text buffer             */

    /* ── Cursor position (zero-based) ───────────────────────────── */
    int         cursor_row;     /* Current row in the buffer        */
    int         cursor_col;     /* Current column in the buffer     */

    /* ── Viewport (which part of the buffer is visible) ─────────── */
    int         scroll_row;     /* First visible row of the buffer  */
    int         scroll_col;     /* First visible column             */

    /* ── Terminal dimensions (updated on SIGWINCH) ───────────────── */
    int         term_rows;      /* Terminal height in characters    */
    int         term_cols;      /* Terminal width in characters     */

    /* ── File ───────────────────────────────────────────────────── */
    char        filename[256];  /* Path of the open file, or ""    */

    /* ── Undo/Redo ──────────────────────────────────────────────── */
    UndoStack  *undo;           /* The undo/redo stack              */

    /* ── Search ─────────────────────────────────────────────────── */
    SearchCtx  *search;         /* Current search query and state   */

    /* ── Status message ─────────────────────────────────────────── */
    char        status_msg[256];/* Displayed in the status bar      */
    int         status_msg_dirty; /* 1 if status_msg should be cleared on next keypress */

    /* ── Configuration ──────────────────────────────────────────── */
    int         tab_size;       /* Number of spaces per Tab key     */
    int         auto_bracket;   /* 1 to enable auto bracket completion */
    int         smart_indent;   /* 1 to enable smart indentation    */

    /* ── Loop control ───────────────────────────────────────────── */
    int         running;        /* 0 to exit the event loop         */
} EditorState;
```

### Why no global variables?

Some editors store cursor position, buffer pointer, etc. in global variables.
This is simpler to write but harder to test and reason about.

By passing `EditorState *state` to every function, we can:

1. Write unit tests that create a fake `EditorState` with known contents
2. See the complete state in a debugger by printing `*state`
3. In the future, support multiple open buffers by having multiple `EditorState`s

The cost is that every function signature includes the `state` pointer. This
is a minor inconvenience and a worthwhile trade.

---

## The Event Loop

```c
void editor_run(EditorState *state)
{
    editor_refresh_screen(state);   /* Draw initial state */

    while (state->running)
    {
        KeyEvent key = terminal_read_key();   /* Block until input */

        /* Clear any transient status messages on the next keypress */
        if (state->status_msg_dirty)
        {
            state->status_msg[0]    = '\0';
            state->status_msg_dirty = 0;
        }

        editor_handle_key(state, key);        /* Dispatch */
        editor_refresh_screen(state);         /* Redraw */
    }
}
```

This loop is the heartbeat of the editor. Let's examine each step:

### `terminal_read_key()`

This call blocks until a key is available. The process sleeps here. When the
user presses a key, the OS delivers bytes to our `read()` call, which decodes
them into a `KeyEvent` and returns.

A `KeyEvent` is a small struct:

```c
typedef enum KeyType
{
    KEY_CHAR,       /* A printable character: 'a', 'Z', '5', etc.  */
    KEY_CTRL,       /* A control key: Ctrl+S, Ctrl+Z, etc.         */
    KEY_ARROW_UP,
    KEY_ARROW_DOWN,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_ESCAPE,
    KEY_UNKNOWN,
} KeyType;

typedef struct KeyEvent
{
    KeyType type;
    int     value;  /* For KEY_CHAR: the character code
                       For KEY_CTRL: the control code (e.g., 19 for Ctrl+S) */
} KeyEvent;
```

### `editor_handle_key()`

This function is a large dispatch: given a `KeyEvent`, call the appropriate
editor action.

```c
void editor_handle_key(EditorState *state, KeyEvent key)
{
    switch (key.type)
    {
        case KEY_CHAR:
            editor_insert_char(state, (char)key.value);
            break;

        case KEY_ENTER:
            editor_insert_newline(state);
            break;

        case KEY_BACKSPACE:
            editor_do_backspace(state);
            break;

        case KEY_DELETE:
            editor_do_delete(state);
            break;

        case KEY_ARROW_UP:
            editor_move_cursor(state, -1, 0);
            break;

        case KEY_ARROW_DOWN:
            editor_move_cursor(state, +1, 0);
            break;

        case KEY_ARROW_LEFT:
            editor_move_cursor(state, 0, -1);
            break;

        case KEY_ARROW_RIGHT:
            editor_move_cursor(state, 0, +1);
            break;

        case KEY_HOME:
            state->cursor_col = 0;
            break;

        case KEY_END:
            state->cursor_col = buffer_get_line_length(state->buf, state->cursor_row);
            break;

        case KEY_PAGE_UP:
            editor_scroll(state, -(state->term_rows - 2));
            break;

        case KEY_PAGE_DOWN:
            editor_scroll(state, +(state->term_rows - 2));
            break;

        case KEY_CTRL:
            editor_handle_ctrl(state, key.value);
            break;

        default:
            break;
    }
}
```

### `editor_handle_ctrl()`

Ctrl+key combinations are handled here:

```c
void editor_handle_ctrl(EditorState *state, int ctrl_code)
{
    switch (ctrl_code)
    {
        case CTRL('s'): editor_do_save(state);       break;
        case CTRL('o'): editor_do_open(state);       break;
        case CTRL('q'): editor_do_quit(state);       break;
        case CTRL('z'): editor_do_undo(state);       break;
        case CTRL('y'): editor_do_redo(state);       break;
        case CTRL('f'): editor_do_find(state);       break;
        case CTRL('n'): editor_do_find_next(state);  break;
        case CTRL('p'): editor_do_find_prev(state);  break;
        case CTRL('g'): editor_do_goto_line(state);  break;
        case CTRL('r'): editor_do_replace(state);    break;
    }
}
```

`CTRL('s')` is the macro `((s) & 0x1f)` — standard C idiom for the ctrl code
of a letter. Ctrl+S sends byte 0x13 (decimal 19), which is `'s' & 0x1f`.

---

## Key Dispatch Table

The complete mapping of keys to actions:

| Key          | Action                                          |
|--------------|-------------------------------------------------|
| Printable    | Insert character (with auto-bracket if enabled) |
| Enter        | Insert newline (with smart indent if enabled)   |
| Backspace    | Delete char before cursor (merge lines if at col 0) |
| Delete       | Delete char after cursor                        |
| Up arrow     | Move cursor up one row                          |
| Down arrow   | Move cursor down one row                        |
| Left arrow   | Move cursor left one col (wraps to prev line end) |
| Right arrow  | Move cursor right one col (wraps to next line start) |
| Home         | Move cursor to column 0                         |
| End          | Move cursor to end of current line              |
| Page Up      | Scroll viewport up by terminal height           |
| Page Down    | Scroll viewport down by terminal height         |
| Ctrl+S       | Save file                                       |
| Ctrl+O       | Open file (prompt for filename)                 |
| Ctrl+Q       | Quit (warns if unsaved changes)                 |
| Ctrl+Z       | Undo                                            |
| Ctrl+Y       | Redo                                            |
| Ctrl+F       | Find (prompt for search query)                  |
| Ctrl+N       | Find next match                                 |
| Ctrl+P       | Find previous match                             |
| Ctrl+G       | Go to line (prompt for line number)             |
| Ctrl+R       | Replace                                         |

---

## Cursor and Viewport Management

The cursor has two coordinate systems:

1. **Buffer coordinates:** `(cursor_row, cursor_col)` — position in the document.
   Row 0, Col 0 is the top-left of the file.

2. **Screen coordinates:** `(screen_row, screen_col)` — position on the terminal.
   Screen row 0 is the top of the terminal window.

The relationship is:

```
screen_row = cursor_row - scroll_row
screen_col = cursor_col - scroll_col
```

### Scrolling

When the cursor moves past the visible viewport, the viewport scrolls:

```c
void editor_scroll_to_cursor(EditorState *state)
{
    /* Scroll up if cursor is above the viewport */
    if (state->cursor_row < state->scroll_row)
        state->scroll_row = state->cursor_row;

    /* Scroll down if cursor is below the viewport */
    /* (leave 1 row for the status bar) */
    if (state->cursor_row >= state->scroll_row + state->term_rows - 1)
        state->scroll_row = state->cursor_row - state->term_rows + 2;

    /* Scroll left */
    if (state->cursor_col < state->scroll_col)
        state->scroll_col = state->cursor_col;

    /* Scroll right */
    if (state->cursor_col >= state->scroll_col + state->term_cols)
        state->scroll_col = state->cursor_col - state->term_cols + 1;
}
```

### Cursor clamping

When moving the cursor vertically (up/down arrow), the column must be
clamped to the length of the new row, because different rows have different
lengths:

```c
void editor_move_cursor(EditorState *state, int drow, int dcol)
{
    int new_row = state->cursor_row + drow;
    int new_col = state->cursor_col + dcol;

    /* Clamp row to buffer bounds */
    if (new_row < 0) new_row = 0;
    if (new_row >= state->buf->line_count) new_row = state->buf->line_count - 1;

    /* Clamp column to line length */
    int line_len = buffer_get_line_length(state->buf, new_row);
    if (new_col < 0)         new_col = 0;
    if (new_col > line_len)  new_col = line_len;

    state->cursor_row = new_row;
    state->cursor_col = new_col;

    editor_scroll_to_cursor(state);
}
```

---

## Smart Editing Features

### Auto Bracket Completion

When `state->auto_bracket == 1` and the user types an opening bracket:

```
( [ { " '
```

The editor inserts the matching closing bracket and moves the cursor inside:

```c
void editor_insert_char(EditorState *state, char c)
{
    if (state->auto_bracket)
    {
        char closing = 0;
        if      (c == '(')  closing = ')';
        else if (c == '[')  closing = ']';
        else if (c == '{')  closing = '}';
        else if (c == '"')  closing = '"';
        else if (c == '\'') closing = '\'';

        if (closing)
        {
            undo_push(state->undo, UNDO_INSERT_PAIR, state->cursor_row,
                      state->cursor_col, c, closing);

            buffer_insert_char(state->buf, state->cursor_row,
                               state->cursor_col, c);
            buffer_insert_char(state->buf, state->cursor_row,
                               state->cursor_col + 1, closing);

            /* Cursor sits BETWEEN the two brackets */
            state->cursor_col += 1;
            return;
        }
    }

    /* Normal single-character insert */
    undo_push(state->undo, UNDO_INSERT, state->cursor_row,
              state->cursor_col, c, 0);
    buffer_insert_char(state->buf, state->cursor_row, state->cursor_col, c);
    state->cursor_col += 1;
}
```

**Edge case:** If the user types a `"` when the cursor is already *on* a `"`,
we do not insert a new pair — we just move the cursor past the existing `"`.
This handles the case of typing the closing delimiter yourself.

### Smart Indentation

When Enter is pressed, the new line should be indented to at least the same
level as the current line.

```c
void editor_insert_newline(EditorState *state)
{
    int row = state->cursor_row;
    int col = state->cursor_col;

    /* Count the leading whitespace of the current line */
    const char *line  = buffer_get_line(state->buf, row);
    int         indent = 0;

    while (line[indent] == ' ' || line[indent] == '\t')
        indent++;

    /* Check if we should increase indent:
       - The current line ends with '{'
       - The last non-whitespace char before the cursor is ':' (case label)
       - The line starts with 'if', 'for', 'while', etc.
       For v1, we only handle the '{' case. */
    int extra_indent = 0;
    if (col > 0 && line[col - 1] == '{')
        extra_indent = state->tab_size;

    /* Push undo: this is a compound operation (split line + add indent) */
    undo_push_newline(state->undo, row, col, indent + extra_indent);

    /* Split the line at col */
    buffer_insert_line_at(state->buf, row, col);

    /* Add the indentation to the new line */
    for (int i = 0; i < indent + extra_indent; i++)
        buffer_insert_char(state->buf, row + 1, i, ' ');

    /* Move cursor to the new line, after the indentation */
    state->cursor_row = row + 1;
    state->cursor_col = indent + extra_indent;
}
```

---

## Mode Design

tEdit Version 1 is **modeless**: there is no insert/normal/visual mode like
Vim. You are always in insert mode. This is the design of nano, gedit, and
most modern editors.

Rationale: Modal editors are powerful but have a steeper learning curve.
Since tEdit is also a learning project, we avoid adding modality complexity
until the user has mastered the core architecture.

Version 2.0 may add an optional modal mode as an experimental feature.

The `EditorState` struct has no `mode` field in v1. If modes are added, it
would be:

```c
typedef enum EditorMode { MODE_INSERT, MODE_NORMAL, MODE_VISUAL } EditorMode;
/* Added to EditorState when needed */
```

---

## Status Bar

The status bar occupies the bottom row of the terminal. It is always redrawn
as part of `editor_refresh_screen()`.

```
Format:
  ┌──────────────────────────────────────────────────────────┐
  │ main.c [+] | Ln 42, Col 15 | 200 lines | Tab: 4         │
  └──────────────────────────────────────────────────────────┘

Fields:
  filename     — the current file path, or "[No Name]" for unsaved buffers
  [+]          — shown if the buffer has been modified since last save
  Ln N, Col M  — current cursor position (1-based for display, 0-based internally)
  N lines      — total lines in the buffer
  Tab: N       — current tab size setting
```

When a status message is set (e.g., "File saved." or "Search: not found"), it
replaces the normal status bar for one keypress, then reverts.

The `status_msg_dirty` flag controls this:

```c
/* Set a transient message */
void editor_set_status(EditorState *state, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(state->status_msg, sizeof(state->status_msg), fmt, ap);
    va_end(ap);
    state->status_msg_dirty = 1;  /* Clear on next keypress */
}
```
