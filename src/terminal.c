#define _POSIX_C_SOURCE 200809L  /* Enables POSIX APIs on Linux; harmless on Windows */

#include "terminal.h"
#include "editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#ifndef _WIN32
#  include <unistd.h>
#  include <termios.h>
#  include <sys/ioctl.h>
#  include <signal.h>
#else
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>
#  ifndef STDOUT_FILENO
#    define STDOUT_FILENO 1
#  endif
#endif

#define WB_INITIAL_CAP 8192

typedef struct WriteBuf { char *data; int len; int cap; } WriteBuf;

static int _wb_init(WriteBuf *wb)
{
    wb->data = malloc(WB_INITIAL_CAP);
    if (!wb->data) return -1;
    wb->len = 0;
    wb->cap = WB_INITIAL_CAP;
    return 0;
}

static void _wb_append(WriteBuf *wb, const char *src, int len)
{
    if (len <= 0) return;
    if (wb->len + len > wb->cap)
    {
        int nc = wb->cap * 2;
        while (nc < wb->len + len) nc *= 2;
        char *nd = realloc(wb->data, (size_t)nc);
        if (!nd) return;
        wb->data = nd;
        wb->cap  = nc;
    }
    memcpy(wb->data + wb->len, src, (size_t)len);
    wb->len += len;
}

static void _wb_appendf(WriteBuf *wb, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0 && n < (int)sizeof(tmp))
        _wb_append(wb, tmp, n);
}

static void _wb_free(WriteBuf *wb)
{
    free(wb->data);
    wb->data = NULL;
    wb->len = wb->cap = 0;
}

#ifndef _WIN32
static struct termios        original_termios;
static struct termios        raw_termios;
static volatile sig_atomic_t terminal_resized = 0;

#else
static HANDLE        win_hIn         = INVALID_HANDLE_VALUE;
static HANDLE        win_hOut        = INVALID_HANDLE_VALUE;
static DWORD         win_orig_in     = 0;
static DWORD         win_orig_out    = 0;
static volatile int  terminal_resized = 0;
static int           win_last_rows   = 0;
static int           win_last_cols   = 0;
#endif

static int raw_mode_active = 0;

#ifndef _WIN32
static void _sigwinch_handler(int sig)
{
    (void)sig;
    terminal_resized = 1;
}
#endif

void terminal_get_size(int *rows, int *cols)
{
#ifndef _WIN32
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0)
    {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return;
    }
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B\x1b[6n", 12) != 12)
    { *rows = 24; *cols = 80; return; }
    char buf[32]; int i = 0;
    while (i < (int)sizeof(buf)-1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i++] == 'R') break;
    }
    buf[i] = '\0';
    if (buf[0]=='\x1b' && buf[1]=='[' && sscanf(buf+2,"%d;%d",rows,cols)==2)
        return;
    *rows = 24; *cols = 80;

#else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h = (win_hOut != INVALID_HANDLE_VALUE)
               ? win_hOut : GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(h, &csbi))
    {
        *cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
    }
    else { *rows = 24; *cols = 80; }
#endif
}

void terminal_init(int *rows, int *cols)
{
#ifndef _WIN32
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
    { fprintf(stderr,"cedit: tcgetattr: %s\n",strerror(errno)); exit(1); }

    raw_termios = original_termios;
    raw_termios.c_iflag &= ~(unsigned)(IXON|ICRNL|BRKINT|INPCK|ISTRIP);
    raw_termios.c_oflag &= ~(unsigned)(OPOST);
    raw_termios.c_cflag |=  (unsigned)(CS8);
    raw_termios.c_lflag &= ~(unsigned)(ECHO|ICANON|ISIG|IEXTEN);
    raw_termios.c_cc[VMIN]  = 1;
    raw_termios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios) == -1)
    { fprintf(stderr,"cedit: tcsetattr: %s\n",strerror(errno)); exit(1); }

    atexit(terminal_cleanup);
    signal(SIGWINCH, _sigwinch_handler);

#else
    win_hIn  = GetStdHandle(STD_INPUT_HANDLE);
    win_hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    if (win_hIn  == INVALID_HANDLE_VALUE ||
        win_hOut == INVALID_HANDLE_VALUE)
    { fprintf(stderr,"cedit: GetStdHandle failed\n"); exit(1); }

    GetConsoleMode(win_hIn,  &win_orig_in);
    GetConsoleMode(win_hOut, &win_orig_out);

    DWORD new_in = (win_orig_in
                    & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT
                               | ENABLE_PROCESSED_INPUT
                               | ENABLE_QUICK_EDIT_MODE))
                   | ENABLE_WINDOW_INPUT
                   | ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(win_hIn, new_in);

    DWORD new_out = win_orig_out
                    | ENABLE_PROCESSED_OUTPUT
                    | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(win_hOut, new_out);

    atexit(terminal_cleanup);

    terminal_get_size(rows, cols);
    win_last_rows = *rows;
    win_last_cols = *cols;

    raw_mode_active = 1;
    return;
#endif

    raw_mode_active = 1;
    terminal_get_size(rows, cols);
}

void terminal_cleanup(void)
{
    if (!raw_mode_active) return;
    raw_mode_active = 0;

    { int r = write(STDOUT_FILENO, "\x1b[0m\x1b[?25h", 10); (void)r; }

#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
#else
    if (win_hIn  != INVALID_HANDLE_VALUE) SetConsoleMode(win_hIn,  win_orig_in);
    if (win_hOut != INVALID_HANDLE_VALUE) SetConsoleMode(win_hOut, win_orig_out);
#endif
}

#ifndef _WIN32

static KeyEvent _terminal_read_escape_sequence(void)
{
    char seq[8];
    int  n;

    {
        struct termios t = raw_termios;
        t.c_cc[VMIN]  = 0;
        t.c_cc[VTIME] = 1;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
    n = read(STDIN_FILENO, &seq[0], 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);

    if (n != 1) return (KeyEvent){ KEY_ESCAPE, 0 };

    if (seq[0] == '[')
    {
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return (KeyEvent){ KEY_UNKNOWN, 0 };

        if (seq[1] >= '0' && seq[1] <= '9')
        {
            if (read(STDIN_FILENO, &seq[2], 1) != 1)
                return (KeyEvent){ KEY_UNKNOWN, 0 };
            if (seq[2] == '~')
            {
                switch (seq[1]) {
                    case '1': return (KeyEvent){ KEY_HOME,      0 };
                    case '3': return (KeyEvent){ KEY_DELETE,    0 };
                    case '4': return (KeyEvent){ KEY_END,       0 };
                    case '5': return (KeyEvent){ KEY_PAGE_UP,   0 };
                    case '6': return (KeyEvent){ KEY_PAGE_DOWN, 0 };
                    case '7': return (KeyEvent){ KEY_HOME,      0 };
                    case '8': return (KeyEvent){ KEY_END,       0 };
                    default:  return (KeyEvent){ KEY_UNKNOWN,   0 };
                }
            }
            return (KeyEvent){ KEY_UNKNOWN, 0 };
        }
        switch (seq[1]) {
            case 'A': return (KeyEvent){ KEY_ARROW_UP,    0 };
            case 'B': return (KeyEvent){ KEY_ARROW_DOWN,  0 };
            case 'C': return (KeyEvent){ KEY_ARROW_RIGHT, 0 };
            case 'D': return (KeyEvent){ KEY_ARROW_LEFT,  0 };
            case 'H': return (KeyEvent){ KEY_HOME,        0 };
            case 'F': return (KeyEvent){ KEY_END,         0 };
            default:  return (KeyEvent){ KEY_UNKNOWN,     0 };
        }
    }
    else if (seq[0] == 'O')
    {
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return (KeyEvent){ KEY_UNKNOWN, 0 };
        switch (seq[1]) {
            case 'A': return (KeyEvent){ KEY_ARROW_UP,    0 };
            case 'B': return (KeyEvent){ KEY_ARROW_DOWN,  0 };
            case 'C': return (KeyEvent){ KEY_ARROW_RIGHT, 0 };
            case 'D': return (KeyEvent){ KEY_ARROW_LEFT,  0 };
            case 'H': return (KeyEvent){ KEY_HOME,        0 };
            case 'F': return (KeyEvent){ KEY_END,         0 };
            default:  return (KeyEvent){ KEY_UNKNOWN,     0 };
        }
    }
    return (KeyEvent){ KEY_UNKNOWN, 0 };
}
#endif

KeyEvent terminal_read_key(void)
{
#ifndef _WIN32
    unsigned char c;
    int nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EINTR)
        { fprintf(stderr,"cedit: read: %s\n",strerror(errno)); exit(1); }
    }
    if (c == '\x1b') return _terminal_read_escape_sequence();
    if (c == '\r')   return (KeyEvent){ KEY_ENTER,     0 };
    if (c == 127)    return (KeyEvent){ KEY_BACKSPACE,  0 };
    if (c == '\t')   return (KeyEvent){ KEY_CHAR,     '\t' };
    if (c >= 1 && c <= 26) return (KeyEvent){ KEY_CTRL, c };
    if (c >= 32 && c <= 126) return (KeyEvent){ KEY_CHAR, c };
    return (KeyEvent){ KEY_UNKNOWN, (int)c };

#else
    while (1)
    {
        INPUT_RECORD ir;
        DWORD        count = 0;

        if (!ReadConsoleInput(win_hIn, &ir, 1, &count) || count == 0)
            continue;

        if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            terminal_resized = 1;
            continue;
        }

        if (ir.EventType != KEY_EVENT) continue;
        if (!ir.Event.KeyEvent.bKeyDown) continue;

        WORD  vk   = ir.Event.KeyEvent.wVirtualKeyCode;
        char  ch   = ir.Event.KeyEvent.uChar.AsciiChar;
        DWORD ctrl = ir.Event.KeyEvent.dwControlKeyState;

        int is_ctrl = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

        switch (vk)
        {
            case VK_UP:     return (KeyEvent){ KEY_ARROW_UP,    0 };
            case VK_DOWN:   return (KeyEvent){ KEY_ARROW_DOWN,  0 };
            case VK_LEFT:   return (KeyEvent){ KEY_ARROW_LEFT,  0 };
            case VK_RIGHT:  return (KeyEvent){ KEY_ARROW_RIGHT, 0 };
            case VK_HOME:   return (KeyEvent){ KEY_HOME,        0 };
            case VK_END:    return (KeyEvent){ KEY_END,         0 };
            case VK_PRIOR:  return (KeyEvent){ KEY_PAGE_UP,     0 }; /* PgUp */
            case VK_NEXT:   return (KeyEvent){ KEY_PAGE_DOWN,   0 }; /* PgDn */
            case VK_DELETE: return (KeyEvent){ KEY_DELETE,      0 };
            case VK_BACK:   return (KeyEvent){ KEY_BACKSPACE,   0 };
            case VK_RETURN: return (KeyEvent){ KEY_ENTER,       0 };
            case VK_ESCAPE: return (KeyEvent){ KEY_ESCAPE,      0 };
            case VK_TAB:    return (KeyEvent){ KEY_CHAR,       '\t'};
            default: break;
        }

        if (is_ctrl && ch >= 1 && ch <= 26)
            return (KeyEvent){ KEY_CTRL, ch };

        if (ch >= 32 && ch <= 126)
            return (KeyEvent){ KEY_CHAR, ch };

    }
#endif
}

int terminal_was_resized(void)
{
#ifndef _WIN32
    if (terminal_resized) { terminal_resized = 0; return 1; }
    return 0;
#else
    if (terminal_resized) { terminal_resized = 0; return 1; }
    {
        int r = 24, c = 80;
        terminal_get_size(&r, &c);
        if (r != win_last_rows || c != win_last_cols)
        {
            win_last_rows = r;
            win_last_cols = c;
            return 1;
        }
    }
    return 0;
#endif
}

static void _render_row(WriteBuf *wb, const EditorState *state, int buf_row)
{
    _wb_append(wb, "\x1b[K", 3);

    if (buf_row >= buffer_get_line_count(state->buf))
    {
        _wb_append(wb, "\x1b[2m~\x1b[0m", 9);
        return;
    }

    const char *line     = buffer_get_line(state->buf, buf_row);
    int         line_len = buffer_get_line_length(state->buf, buf_row);
    int         vs       = state->scroll_col;
    int         ve       = vs + state->term_cols;

    if (vs >= line_len) return;
    if (ve > line_len)  ve = line_len;

    int hl = 0, hl_s = 0, hl_e = 0;
    if (state->search &&
        state->search->highlight_row == buf_row &&
        state->search->highlight_len > 0)
    {
        hl   = 1;
        hl_s = state->search->highlight_col;
        hl_e = hl_s + state->search->highlight_len;
    }

    if (!hl)
    {
        _wb_append(wb, line + vs, ve - vs);
        return;
    }

    { int s=vs, e=(hl_s>vs)?hl_s:vs; if(e>ve)e=ve; if(e>s) _wb_append(wb,line+s,e-s); }
    {
        int s=(hl_s>vs)?hl_s:vs, e=(hl_e<ve)?hl_e:ve;
        if (e>s) {
            _wb_append(wb,"\x1b[43m\x1b[30m",10);
            _wb_append(wb,line+s,e-s);
            _wb_append(wb,"\x1b[0m",4);
        }
    }
    { int s=(hl_e>vs)?hl_e:vs, e=ve; if(e>s) _wb_append(wb,line+s,e-s); }
}

static void _render_status_bar(WriteBuf *wb, const EditorState *state)
{
    _wb_append(wb, "\x1b[7m", 4);

    char bar[512];
    int  blen;

    if (state->status_msg[0] != '\0')
    {
        blen = snprintf(bar, sizeof(bar), " %s", state->status_msg);
    }
    else
    {
        const char *fname    = state->filename[0] ? state->filename : "[No Name]";
        const char *modified = buffer_is_modified(state->buf) ? " [+]" : "";
        blen = snprintf(bar, sizeof(bar),
                        " %s%s | Ln %d, Col %d | %d lines | Tab: %d  ",
                        fname, modified,
                        state->cursor_row + 1, state->cursor_col + 1,
                        buffer_get_line_count(state->buf),
                        state->tab_size);
    }

    if (blen < 0) blen = 0;
    if (blen > (int)sizeof(bar)-1) blen = (int)sizeof(bar)-1;

    int tw = (blen <= state->term_cols) ? blen : state->term_cols;
    _wb_append(wb, bar, tw);
    { int i; for (i = tw; i < state->term_cols; i++) _wb_append(wb," ",1); }
    _wb_append(wb, "\x1b[0m", 4);
}

void terminal_render(EditorState *state)
{
    WriteBuf wb;
    if (_wb_init(&wb) == -1)
    {
        int r = write(STDOUT_FILENO,"\x1b[2J\x1b[HOut of memory.\r\n",22);
        (void)r; return;
    }

    _wb_append(&wb, "\x1b[?25l", 6);
    _wb_append(&wb, "\x1b[H",    3);

    int visible = state->term_rows - 1;
    {
        int sr;
        for (sr = 0; sr < visible; sr++)
        {
            _render_row(&wb, state, state->scroll_row + sr);
            _wb_append(&wb, "\r\n", 2);
        }
    }

    _render_status_bar(&wb, state);

    
    _wb_appendf(&wb, "\x1b[%d;%dH",
                (state->cursor_row - state->scroll_row) + 1,
                (state->cursor_col - state->scroll_col) + 1);

    _wb_append(&wb, "\x1b[?25h", 6);

    { int r = write(STDOUT_FILENO, wb.data, (size_t)wb.len); (void)r; }
    _wb_free(&wb);
}

int terminal_prompt(EditorState *state, const char *prompt,
                    char *out, int out_len)
{
    char input[EDITOR_STATUS_MSG_MAX];
    int  ilen = 0;
    char disp[EDITOR_STATUS_MSG_MAX];
    memset(input, 0, sizeof(input));
    if (out_len > 0) out[0] = '\0';

    while (1)
    {
        snprintf(disp, sizeof(disp), "%s%.*s", prompt, ilen, input);
        strncpy(state->status_msg, disp, (size_t)(EDITOR_STATUS_MSG_MAX-1));
        state->status_msg[EDITOR_STATUS_MSG_MAX-1] = '\0';
        state->status_msg_dirty = 0;
        terminal_render(state);

        KeyEvent key = terminal_read_key();

        if (key.type == KEY_ENTER)
        {
            if (ilen == 0) { state->status_msg[0]='\0'; return 0; }
            strncpy(out, input, (size_t)(out_len-1));
            out[out_len-1] = '\0';
            state->status_msg[0] = '\0';
            return 1;
        }
        else if (key.type == KEY_ESCAPE)
        {
            state->status_msg[0] = '\0';
            return 0;
        }
        else if (key.type == KEY_BACKSPACE)
        {
            if (ilen > 0) input[--ilen] = '\0';
        }
        else if (key.type == KEY_CHAR &&
                 ilen < (int)sizeof(input)-1 &&
                 ilen < out_len-1)
        {
            input[ilen++] = (char)key.value;
            input[ilen]   = '\0';
        }
    }
}
