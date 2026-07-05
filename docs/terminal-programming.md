# Terminal Programming

> This document teaches you how terminal editors actually work at the OS level.
> Read this before reading terminal.c. It will make the code obvious.

---

## Table of Contents

1. [What Is a Terminal?](#what-is-a-terminal)
2. [Cooked vs Raw Mode](#cooked-vs-raw-mode)
3. [Enabling Raw Mode with termios](#enabling-raw-mode-with-termios)
4. [Reading Key Input](#reading-key-input)
5. [ANSI Escape Sequences](#ansi-escape-sequences)
6. [Cursor Movement](#cursor-movement)
7. [Screen Clearing](#screen-clearing)
8. [Colours](#colours)
9. [Getting the Terminal Size](#getting-the-terminal-size)
10. [The Full Rendering Sequence](#the-full-rendering-sequence)
11. [Key Decoding](#key-decoding)
12. [Handling Terminal Resize](#handling-terminal-resize)
13. [Windows Differences](#windows-differences)

---

## What Is a Terminal?

When you open a terminal application (gnome-terminal, iTerm2, Windows Terminal),
you see a window with text. But from a program's perspective, the terminal is
just two streams:

- **stdin**: bytes flowing from keyboard to program
- **stdout**: bytes flowing from program to display

The terminal emulator intercepts these streams and:
- Translates your keypresses into bytes (sent to stdin)
- Reads bytes from stdout and renders them as characters on screen

Most of the "intelligence" is in the terminal emulator itself. Your program
just reads and writes bytes.

```
  ┌────────────────────────────────────────────────────────┐
  │                   Terminal Emulator                    │
  │                                                        │
  │  Keyboard ──► [key decode] ──► stdin ──► our program  │
  │                                                        │
  │  our program ──► stdout ──► [render] ──► Screen       │
  └────────────────────────────────────────────────────────┘
```

---

## Cooked vs Raw Mode

### Cooked Mode (default)

By default, the terminal is in **cooked mode** (also called "canonical mode").
In cooked mode:

- The terminal itself handles backspace, Ctrl+C, Ctrl+Z, etc.
- Your program does NOT receive keypresses one-by-one.
- Your program only receives input after the user presses **Enter**.
- The terminal echoes what the user types automatically.

This is how `scanf()` and `fgets()` work. Cooked mode is fine for simple
command-line programs that read line-by-line.

```
Cooked mode timeline:
  User types: h e l l o Enter
  Program sees at once: "hello\n"   ← only after Enter
```

### Raw Mode

In **raw mode**:

- Keypresses are delivered to the program **immediately** (one byte at a time).
- The terminal does NOT handle backspace, Ctrl+C, etc. Your program handles them.
- The terminal does NOT echo characters. Your program draws what it wants.

This is what text editors need. When the user presses 'h', tEdit must:
1. Immediately receive the byte `'h'`
2. Insert it into the buffer
3. Redraw the screen

With cooked mode, this is impossible.

---

## Enabling Raw Mode with termios

On Linux/macOS, terminal settings are controlled via the `termios` struct and
the `tcsetattr()` / `tcgetattr()` functions from `<termios.h>`.

```c
#include <termios.h>
#include <unistd.h>

/* We save the original terminal settings so we can restore them on exit */
static struct termios original_termios;

void terminal_enter_raw_mode(void)
{
    /* Get current terminal settings */
    tcgetattr(STDIN_FILENO, &original_termios);

    struct termios raw = original_termios;

    /*
     * Input flags to disable:
     *
     * IXON    — Disable Ctrl+S / Ctrl+Q flow control.
     *           (We want Ctrl+S for save, Ctrl+Q for quit.)
     *
     * ICRNL   — Disable Ctrl+M / Enter translation.
     *           (Normally Enter sends \r\n; we want raw \r.)
     *
     * BRKINT  — Disable break signal (Ctrl+C sending SIGINT).
     * INPCK   — Disable parity checking (legacy, always disable).
     * ISTRIP  — Disable stripping of 8th bit (legacy).
     */
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);

    /*
     * Output flags to disable:
     *
     * OPOST   — Disable output processing.
     *           (Normally \n becomes \r\n automatically. We handle this ourselves
     *            by writing \r\n explicitly in our escape sequences.)
     */
    raw.c_oflag &= ~(OPOST);

    /*
     * Control flags:
     *
     * CS8     — Set character size to 8 bits (standard, always do this).
     */
    raw.c_cflag |= (CS8);

    /*
     * Local flags to disable:
     *
     * ECHO    — Disable automatic echoing of typed characters.
     *           (We draw characters ourselves after inserting into buffer.)
     *
     * ICANON  — Disable canonical (cooked) mode.
     *           (The key change: now we read byte-by-byte, not line-by-line.)
     *
     * IEXTEN  — Disable Ctrl+V (literal next character) processing.
     *
     * ISIG    — Disable Ctrl+C (SIGINT) and Ctrl+Z (SIGTSTP) signal handling.
     *           (We want to handle these ourselves.)
     */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /*
     * Minimum bytes for read() to return, and timeout:
     *
     * VMIN=1  — read() returns after 1 byte is available (wait for input)
     * VTIME=0 — No timeout (wait indefinitely for input)
     *
     * Together: read() blocks until exactly 1 byte is available, then
     * returns. This is the correct setting for a blocking editor loop.
     */
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    /* Apply the new settings immediately */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void terminal_exit_raw_mode(void)
{
    /* Restore the original settings */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}
```

**Critical:** We must call `terminal_exit_raw_mode()` before the program
exits, or the user's terminal will be left in raw mode — which makes it look
broken (no echo, Enter doesn't work, etc.).

We register an `atexit()` handler to guarantee this even on crashes:

```c
atexit(terminal_exit_raw_mode);
```

---

## Reading Key Input

In raw mode, `read(STDIN_FILENO, &c, 1)` reads exactly 1 byte at a time.

Simple characters (letters, numbers, symbols) each produce 1 byte.
Control characters produce 1 byte.
But special keys (arrows, F-keys, Page Up/Down, etc.) produce **multiple bytes**
that begin with the escape character (`\x1b`, decimal 27, often written as ESC).

Our `terminal_read_key()` function handles this:

```c
KeyEvent terminal_read_key(void)
{
    char c;
    read(STDIN_FILENO, &c, 1);

    if (c != '\x1b')
    {
        /* Simple byte: printable character or control character */
        if (c >= 32 && c < 127)
        {
            /* Printable ASCII */
            return (KeyEvent){ KEY_CHAR, c };
        }
        else if (c == '\r')
        {
            /* Enter key sends \r in raw mode */
            return (KeyEvent){ KEY_ENTER, 0 };
        }
        else if (c == 127)
        {
            /* Backspace sends DEL (0x7f) in most terminals */
            return (KeyEvent){ KEY_BACKSPACE, 0 };
        }
        else if (c >= 1 && c <= 26)
        {
            /* Ctrl+A through Ctrl+Z: value is 1 through 26 */
            return (KeyEvent){ KEY_CTRL, c };
        }
        else
        {
            return (KeyEvent){ KEY_UNKNOWN, c };
        }
    }

    /* c == '\x1b': This is the start of an escape sequence.
       Read more bytes to determine which key was pressed. */
    return _terminal_read_escape_sequence();
}
```

### The Escape Sequence Reader

```c
static KeyEvent _terminal_read_escape_sequence(void)
{
    char seq[8] = {0};

    /* The second byte determines the sequence type */
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
        return (KeyEvent){ KEY_ESCAPE, 0 };   /* Lone ESC key */

    if (seq[0] == '[')
    {
        /* CSI sequence: ESC [ ... */
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return (KeyEvent){ KEY_UNKNOWN, 0 };

        if (seq[1] >= '0' && seq[1] <= '9')
        {
            /* Extended sequence: ESC [ N ~ */
            if (read(STDIN_FILENO, &seq[2], 1) != 1)
                return (KeyEvent){ KEY_UNKNOWN, 0 };

            if (seq[2] == '~')
            {
                switch (seq[1])
                {
                    case '1': return (KeyEvent){ KEY_HOME,      0 };
                    case '3': return (KeyEvent){ KEY_DELETE,    0 };
                    case '4': return (KeyEvent){ KEY_END,       0 };
                    case '5': return (KeyEvent){ KEY_PAGE_UP,   0 };
                    case '6': return (KeyEvent){ KEY_PAGE_DOWN, 0 };
                    case '7': return (KeyEvent){ KEY_HOME,      0 };
                    case '8': return (KeyEvent){ KEY_END,       0 };
                }
            }
        }
        else
        {
            /* Simple CSI sequence: ESC [ A/B/C/D/H/F */
            switch (seq[1])
            {
                case 'A': return (KeyEvent){ KEY_ARROW_UP,    0 };
                case 'B': return (KeyEvent){ KEY_ARROW_DOWN,  0 };
                case 'C': return (KeyEvent){ KEY_ARROW_RIGHT, 0 };
                case 'D': return (KeyEvent){ KEY_ARROW_LEFT,  0 };
                case 'H': return (KeyEvent){ KEY_HOME,        0 };
                case 'F': return (KeyEvent){ KEY_END,         0 };
            }
        }
    }
    else if (seq[0] == 'O')
    {
        /* SS3 sequence: ESC O A/B/C/D/H/F (alternative encoding) */
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return (KeyEvent){ KEY_UNKNOWN, 0 };

        switch (seq[1])
        {
            case 'A': return (KeyEvent){ KEY_ARROW_UP,    0 };
            case 'B': return (KeyEvent){ KEY_ARROW_DOWN,  0 };
            case 'C': return (KeyEvent){ KEY_ARROW_RIGHT, 0 };
            case 'D': return (KeyEvent){ KEY_ARROW_LEFT,  0 };
            case 'H': return (KeyEvent){ KEY_HOME,        0 };
            case 'F': return (KeyEvent){ KEY_END,         0 };
        }
    }

    return (KeyEvent){ KEY_UNKNOWN, 0 };
}
```

---

## ANSI Escape Sequences

ANSI escape sequences are special byte sequences that control the terminal's
display. They always start with `\x1b[` (ESC followed by `[`).

The `[` is called the **CSI** (Control Sequence Introducer).

Here are the sequences tEdit uses:

### Cursor Control

| Sequence               | Effect                                              |
|------------------------|-----------------------------------------------------|
| `\x1b[H`              | Move cursor to top-left (row 1, col 1)             |
| `\x1b[N;MH`           | Move cursor to row N, column M (1-based)           |
| `\x1b[NA`             | Move cursor up N rows                              |
| `\x1b[NB`             | Move cursor down N rows                            |
| `\x1b[NC`             | Move cursor right N columns                        |
| `\x1b[ND`             | Move cursor left N columns                         |
| `\x1b[?25l`           | Hide cursor (prevents flicker during redraw)       |
| `\x1b[?25h`           | Show cursor                                        |
| `\x1b[6n`             | Query cursor position (terminal sends back ESC[R;C]) |

### Erasing

| Sequence               | Effect                                              |
|------------------------|-----------------------------------------------------|
| `\x1b[2J`             | Clear entire screen                                |
| `\x1b[K`              | Erase from cursor to end of line                   |
| `\x1b[2K`             | Erase entire current line                          |

### Colours

| Sequence               | Effect                                              |
|------------------------|-----------------------------------------------------|
| `\x1b[0m`             | Reset all attributes (colour, bold, etc.)          |
| `\x1b[1m`             | Bold                                               |
| `\x1b[7m`             | Reverse video (swap foreground/background)         |
| `\x1b[30m` – `\x1b[37m` | Set foreground colour (black through white)   |
| `\x1b[40m` – `\x1b[47m` | Set background colour (black through white)   |

---

## Cursor Movement

Example: move cursor to row 5, column 10:

```c
char buf[32];
int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 5, 10);
write(STDOUT_FILENO, buf, len);
```

Note: Terminal coordinates are **1-based** (top-left is row 1, col 1).
Our internal buffer coordinates are **0-based** (top-left is row 0, col 0).
We must always add 1 when converting from buffer to terminal coordinates.

---

## Screen Clearing

**Never use `\x1b[2J` (clear entire screen) in a real editor.**

Why? Because it causes a visible flash: the screen goes blank for a moment
before being redrawn. Instead, we clear each line individually as we redraw it.

The correct technique is:
1. At the start of each line's redraw: write `\x1b[K` (erase to end of line)
2. Then write the line content

This means old content is replaced immediately by new content, with no blank
frame in between. No flicker.

---

## Colours

The status bar uses reverse video (`\x1b[7m`) to make it stand out:

```c
/* Start reverse video */
write(STDOUT_FILENO, "\x1b[7m", 4);

/* Write status bar text */
write(STDOUT_FILENO, status_text, strlen(status_text));

/* End reverse video */
write(STDOUT_FILENO, "\x1b[0m", 4);
```

---

## Getting the Terminal Size

The terminal size (rows and columns) is needed to:
1. Know how many lines to render
2. Know how wide lines can be before scrolling
3. Position the status bar at the bottom

```c
#include <sys/ioctl.h>

void terminal_get_size(int *rows, int *cols)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        /* Fallback: use escape sequence to query cursor position.
           Move cursor to bottom-right, then ask where it ended up. */
        write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12);  /* Move far right+down */
        write(STDOUT_FILENO, "\x1b[6n", 4);               /* Query position */

        char buf[32];
        int len = 0;
        /* Read response: ESC [ rows ; cols R */
        while (len < (int)sizeof(buf) - 1)
        {
            if (read(STDIN_FILENO, &buf[len], 1) != 1) break;
            if (buf[len] == 'R') break;
            len++;
        }
        buf[len] = '\0';

        /* Parse ESC [ rows ; cols */
        if (sscanf(buf, "\x1b[%d;%d", rows, cols) != 2)
        {
            /* Absolute fallback */
            *rows = 24;
            *cols = 80;
        }
    }
    else
    {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}
```

---

## The Full Rendering Sequence

```
terminal_render(EditorState *state):
  
  1. \x1b[?25l          ← hide cursor (no flicker during drawing)
  2. \x1b[H             ← move to top-left

  For each visible row r (scroll_row → scroll_row + term_rows - 2):
    3. \x1b[K           ← erase current line
    4. write line r from buffer (clipped to scroll_col, term_cols wide)
    5. \r\n             ← move to next line

  6. Draw status bar:
     \x1b[7m            ← reverse video on
     write status text (filename, modified, pos)
     \x1b[0m            ← reverse video off

  7. \x1b[row;colH      ← position cursor at (cursor_row, cursor_col)
                           (converted to screen coords)
  8. \x1b[?25h          ← show cursor
```

**Performance note:** All of the above is assembled into a single `char` buffer
in memory and sent to `stdout` with ONE `write()` call. Many small `write()`
calls cause system call overhead and potential flickering. One big write is
always better.

---

## Key Decoding

Summary of what bytes different keys produce:

| Key          | Bytes sent to program                  |
|--------------|----------------------------------------|
| 'a'          | `0x61`                                 |
| 'A'          | `0x41`                                 |
| Enter        | `0x0d` (`\r`)                          |
| Backspace    | `0x7f` (DEL) on most terminals         |
| Tab          | `0x09`                                 |
| Escape       | `0x1b`                                 |
| Ctrl+A       | `0x01`                                 |
| Ctrl+S       | `0x13`                                 |
| Ctrl+Z       | `0x1a`                                 |
| Arrow Up     | `0x1b 0x5b 0x41` (ESC [ A)             |
| Arrow Down   | `0x1b 0x5b 0x42` (ESC [ B)             |
| Arrow Right  | `0x1b 0x5b 0x43` (ESC [ C)             |
| Arrow Left   | `0x1b 0x5b 0x44` (ESC [ D)             |
| Page Up      | `0x1b 0x5b 0x35 0x7e` (ESC [ 5 ~)     |
| Page Down    | `0x1b 0x5b 0x36 0x7e` (ESC [ 6 ~)     |
| Home         | `0x1b 0x5b 0x48` or `0x1b 0x5b 0x31 0x7e` |
| End          | `0x1b 0x5b 0x46` or `0x1b 0x5b 0x34 0x7e` |
| Delete       | `0x1b 0x5b 0x33 0x7e` (ESC [ 3 ~)     |
| F1           | `0x1b 0x4f 0x50` (ESC O P)             |

The variation in Home/End encoding is why we must handle multiple escape
sequences for the same logical key.

---

## Handling Terminal Resize

When the user resizes the terminal window, the OS sends our process a
`SIGWINCH` signal.

We register a signal handler:

```c
#include <signal.h>

static volatile int terminal_resized = 0;

static void _sigwinch_handler(int sig)
{
    (void)sig;
    terminal_resized = 1;
}

void terminal_init(void)
{
    signal(SIGWINCH, _sigwinch_handler);
    /* ... */
}
```

In the event loop, after reading a key:

```c
if (terminal_resized)
{
    terminal_resized = 0;
    terminal_get_size(&state->term_rows, &state->term_cols);
    editor_scroll_to_cursor(state);
    /* The screen will be redrawn at the end of the loop iteration */
}
```

Using a `volatile int` flag (rather than doing work inside the signal handler)
is the correct approach: signal handlers should do as little as possible,
since they can interrupt the program at any point.

---

## Windows Differences

On Windows with MinGW, the POSIX `termios` API is not available.
Instead, we use the Win32 console API:

```c
/* Windows raw mode equivalent */
HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

DWORD mode;
GetConsoleMode(hIn, &mode);
SetConsoleMode(hIn, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT
                             | ENABLE_PROCESSED_INPUT));

/* Enable ANSI escape sequences */
GetConsoleMode(hOut, &mode);
SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
```

Once ANSI processing is enabled on Windows, the same escape sequences work
as on Linux. The key reading is different (`_getch()` or `ReadConsoleInput`).

All Windows-specific code lives in `terminal.c` inside `#ifdef _WIN32` guards.
The rest of the codebase sees the same `terminal_enter_raw_mode()` /
`terminal_read_key()` / `terminal_render()` API on both platforms.
