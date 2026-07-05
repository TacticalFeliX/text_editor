#define _POSIX_C_SOURCE 200809L

#include "editor.h"
#include "buffer.h"
#include "terminal.h"
#include "fileio.h"
#include "undo.h"
#include "search.h"

#include <stdio.h>     /* fprintf, stderr, vsnprintf */
#include <stdlib.h>    /* malloc, free, exit */
#include <string.h>    /* strncpy, strlen, memset */
#include <stdarg.h>    /* va_list, va_start, va_end */


EditorState *editor_init(const char *filename)
{
    EditorState *state = malloc(sizeof(EditorState));
    if (state == NULL)
    {
        fprintf(stderr, "cedit: cannot allocate editor state\n");
        return NULL;
    }

    memset(state, 0, sizeof(EditorState));

    state->tab_size     = EDITOR_TAB_SIZE_DEFAULT;
    state->auto_bracket = EDITOR_AUTO_BRACKET_DEFAULT;
    state->smart_indent = EDITOR_SMART_INDENT_DEFAULT;
    state->running      = 1;

    state->buf = buffer_new();
    if (state->buf == NULL)
    {
        fprintf(stderr, "cedit: cannot allocate text buffer\n");
        free(state);
        return NULL;
    }

    state->undo = undo_stack_new(EDITOR_MAX_UNDO_DEFAULT);
    if (state->undo == NULL)
    {
        fprintf(stderr, "cedit: cannot allocate undo stack\n");
        buffer_free(state->buf);
        free(state);
        return NULL;
    }

    state->search = search_ctx_new();
    if (state->search == NULL)
    {
        fprintf(stderr, "cedit: cannot allocate search context\n");
        undo_stack_free(state->undo);
        buffer_free(state->buf);
        free(state);
        return NULL;
    }

    terminal_init(&state->term_rows, &state->term_cols);

    if (filename != NULL && filename[0] != '\0')
    {
        strncpy(state->filename, filename, EDITOR_FILENAME_MAX - 1);
        state->filename[EDITOR_FILENAME_MAX - 1] = '\0';

        if (fileio_open(state->buf, state->filename) != 0)
        {
            editor_set_status(state, "New file: %s", state->filename);
        }
    }

    return state;
}

void editor_free(EditorState *state)
{
    if (state == NULL)
        return;

    search_ctx_free(state->search);
    undo_stack_free(state->undo);
    buffer_free(state->buf);
    free(state);
}

void editor_set_status(EditorState *state, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(state->status_msg, EDITOR_STATUS_MSG_MAX, fmt, ap);
    va_end(ap);

    state->status_msg_dirty = 1;
}

void editor_scroll_to_cursor(EditorState *state)
{
    int text_rows = state->term_rows - 1;

    if (state->cursor_row < state->scroll_row)
    {
        state->scroll_row = state->cursor_row;
    }
    if (state->cursor_row >= state->scroll_row + text_rows)
    {
        state->scroll_row = state->cursor_row - text_rows + 1;
    }

    if (state->cursor_col < state->scroll_col)
    {
        state->scroll_col = state->cursor_col;
    }
    if (state->cursor_col >= state->scroll_col + state->term_cols)
    {
        state->scroll_col = state->cursor_col - state->term_cols + 1;
    }
}

void editor_move_cursor(EditorState *state, int drow, int dcol)
{
    int new_row = state->cursor_row + drow;
    int new_col = state->cursor_col + dcol;

    if (new_row < 0)
        new_row = 0;
    if (new_row >= buffer_get_line_count(state->buf))
        new_row = buffer_get_line_count(state->buf) - 1;

    if (dcol == -1 && state->cursor_col == 0 && state->cursor_row > 0)
    {
        new_row = state->cursor_row - 1;
        new_col = buffer_get_line_length(state->buf, new_row);
        state->preferred_col = new_col;
    }
    else if (dcol == +1 &&
             state->cursor_col == buffer_get_line_length(state->buf, state->cursor_row) &&
             state->cursor_row < buffer_get_line_count(state->buf) - 1)
    {
        new_row = state->cursor_row + 1;
        new_col = 0;
        state->preferred_col = 0;
    }
    else if (drow != 0)
    {
        new_col = state->preferred_col;
    }
    else if (dcol != 0)
    {
        state->preferred_col = new_col;
    }

    {
        int line_len = buffer_get_line_length(state->buf, new_row);
        if (new_col < 0)        new_col = 0;
        if (new_col > line_len) new_col = line_len;
    }

    state->cursor_row = new_row;
    state->cursor_col = new_col;

    editor_scroll_to_cursor(state);
}

void editor_insert_char(EditorState *state, char c)
{
    int row = state->cursor_row;
    int col = state->cursor_col;

    if (c == '\t')
    {
        int i;
        for (i = 0; i < state->tab_size; i++)
        {
            undo_push_insert_char(state->undo, row, col + i, ' ');
            buffer_insert_char(state->buf, row, col + i, ' ');
        }
        state->cursor_col += state->tab_size;
        state->preferred_col = state->cursor_col;
        return;
    }
    if (state->auto_bracket)
    {
        char closing = 0;
        if      (c == '(')  closing = ')';
        else if (c == '[')  closing = ']';
        else if (c == '{')  closing = '}';
        else if (c == '"')  closing = '"';
        else if (c == '\'') closing = '\'';

        if (closing != 0)
        {
            const char *line = buffer_get_line(state->buf, row);
            if ((c == '"' || c == '\'') &&
                col < buffer_get_line_length(state->buf, row) &&
                line[col] == c)
            {
                state->cursor_col++;
                state->preferred_col = state->cursor_col;
                return;
            }

            undo_push_insert_pair(state->undo, row, col, c, closing);
            buffer_insert_char(state->buf, row, col,     c);
            buffer_insert_char(state->buf, row, col + 1, closing);

            state->cursor_col += 1;
            state->preferred_col = state->cursor_col;
            return;
        }
    }

    undo_push_insert_char(state->undo, row, col, c);
    buffer_insert_char(state->buf, row, col, c);

    state->cursor_col++;
    state->preferred_col = state->cursor_col;
}

void editor_insert_newline(EditorState *state)
{
    int         row    = state->cursor_row;
    int         col    = state->cursor_col;
    const char *line   = buffer_get_line(state->buf, row);
    int         indent = 0;

    if (state->smart_indent)
    {
        while (line[indent] == ' ' || line[indent] == '\t')
            indent++;

        if (col > 0 && line[col - 1] == '{')
            indent += state->tab_size;

        if (indent > 64) indent = 64;
    }

    undo_push_split_line(state->undo, row, col, indent);

    buffer_split_line(state->buf, row, col);

    {
        int i;
        for (i = 0; i < indent; i++)
            buffer_insert_char(state->buf, row + 1, i, ' ');
    }

    state->cursor_row    = row + 1;
    state->cursor_col    = indent;
    state->preferred_col = indent;

    editor_scroll_to_cursor(state);
}

void editor_do_backspace(EditorState *state)
{
    int row = state->cursor_row;
    int col = state->cursor_col;

    if (col > 0)
    {
        const char *line    = buffer_get_line(state->buf, row);
        char        deleted = line[col - 1];

        undo_push_delete_char(state->undo, row, col - 1, deleted);
        buffer_delete_char(state->buf, row, col - 1);

        state->cursor_col--;
        state->preferred_col = state->cursor_col;
    }
    else if (row > 0)
    {
        int         prev_len  = buffer_get_line_length(state->buf, row - 1);
        const char *this_line = buffer_get_line(state->buf, row);

        undo_push_merge_lines(state->undo, row - 1, prev_len, this_line);
        buffer_merge_lines(state->buf, row - 1);

        state->cursor_row    = row - 1;
        state->cursor_col    = prev_len;
        state->preferred_col = prev_len;

        editor_scroll_to_cursor(state);
    }
}

void editor_do_delete(EditorState *state)
{
    int row      = state->cursor_row;
    int col      = state->cursor_col;
    int line_len = buffer_get_line_length(state->buf, row);

    if (col < line_len)
    {
        const char *line    = buffer_get_line(state->buf, row);
        char        deleted = line[col];

        undo_push_delete_char(state->undo, row, col, deleted);
        buffer_delete_char(state->buf, row, col);
    }
    else if (row < buffer_get_line_count(state->buf) - 1)
    {
        const char *next_line = buffer_get_line(state->buf, row + 1);
        undo_push_merge_lines(state->undo, row, col, next_line);
        buffer_merge_lines(state->buf, row);
    }
}

void editor_do_save(EditorState *state)
{
    if (state->filename[0] == '\0')
    {
        editor_do_save_as(state);
        return;
    }

    if (fileio_save(state->buf, state->filename) == 0)
    {
        editor_set_status(state, "Saved: %s", state->filename);
    }
    else
    {
        editor_set_status(state, "Error saving file: %s", state->filename);
    }
}

void editor_do_save_as(EditorState *state)
{
    char new_name[EDITOR_FILENAME_MAX];

    int confirmed = terminal_prompt(state, "Save as: ",
                                    new_name, EDITOR_FILENAME_MAX);
    if (!confirmed)
    {
        editor_set_status(state, "Save cancelled.");
        return;
    }

    strncpy(state->filename, new_name, EDITOR_FILENAME_MAX - 1);
    state->filename[EDITOR_FILENAME_MAX - 1] = '\0';

    editor_do_save(state);
}

void editor_do_open(EditorState *state)
{
    char new_name[EDITOR_FILENAME_MAX];

    int confirmed = terminal_prompt(state, "Open file: ",
                                    new_name, EDITOR_FILENAME_MAX);
    if (!confirmed)
    {
        editor_set_status(state, "Open cancelled.");
        return;
    }

    if (buffer_is_modified(state->buf))
        editor_set_status(state, "Warning: unsaved changes discarded.");

    if (fileio_open(state->buf, new_name) == 0)
    {
        strncpy(state->filename, new_name, EDITOR_FILENAME_MAX - 1);
        state->filename[EDITOR_FILENAME_MAX - 1] = '\0';

        state->cursor_row    = 0;
        state->cursor_col    = 0;
        state->preferred_col = 0;
        state->scroll_row    = 0;
        state->scroll_col    = 0;

        undo_stack_free(state->undo);
        state->undo = undo_stack_new(EDITOR_MAX_UNDO_DEFAULT);

        editor_set_status(state, "Opened: %s", state->filename);
    }
    else
    {
        editor_set_status(state, "Cannot open: %s", new_name);
    }
}

void editor_do_quit(EditorState *state)
{
    if (!buffer_is_modified(state->buf))
    {
        state->running = 0;
        return;
    }

    if (state->status_msg[0] != '\0' &&
        state->status_msg[0] == '!')
    {
        state->running = 0;
        return;
    }

    snprintf(state->status_msg, EDITOR_STATUS_MSG_MAX,
             "! Unsaved changes! Press Ctrl+Q again to quit without saving.");
    state->status_msg_dirty = 0;   /* Do NOT auto-clear this one */
}

void editor_do_undo(EditorState *state)
{
    int new_row = state->cursor_row;
    int new_col = state->cursor_col;

    if (undo_undo(state->undo, state->buf, &new_row, &new_col) == 0)
    {
        state->cursor_row    = new_row;
        state->cursor_col    = new_col;
        state->preferred_col = new_col;
        editor_scroll_to_cursor(state);
    }
    else
    {
        editor_set_status(state, "Nothing to undo.");
    }
}

void editor_do_redo(EditorState *state)
{
    int new_row = state->cursor_row;
    int new_col = state->cursor_col;

    if (undo_redo(state->undo, state->buf, &new_row, &new_col) == 0)
    {
        state->cursor_row    = new_row;
        state->cursor_col    = new_col;
        state->preferred_col = new_col;
        editor_scroll_to_cursor(state);
    }
    else
    {
        editor_set_status(state, "Nothing to redo.");
    }
}

static void _jump_to_match(EditorState *state, SearchResult r)
{
    if (!r.found)
    {
        editor_set_status(state, "Not found: %s", state->search->query);
        search_clear_highlight(state->search);
        return;
    }

    state->cursor_row    = r.row;
    state->cursor_col    = r.col;
    state->preferred_col = r.col;
    editor_scroll_to_cursor(state);
}

void editor_do_find(EditorState *state)
{
    char query[SEARCH_MAX_QUERY_LEN];

    int confirmed = terminal_prompt(state, "Find: ",
                                    query, SEARCH_MAX_QUERY_LEN);
    if (!confirmed)
    {
        search_clear_highlight(state->search);
        return;
    }

    search_set_query(state->search, query,
                     state->cursor_row, state->cursor_col);

    SearchResult r = search_find_next(state->search, state->buf);
    _jump_to_match(state, r);
}

void editor_do_find_next(EditorState *state)
{
    if (!state->search->active || state->search->query[0] == '\0')
    {
        editor_set_status(state, "No active search. Use Ctrl+F first.");
        return;
    }
    SearchResult r = search_find_next(state->search, state->buf);
    _jump_to_match(state, r);
}

void editor_do_find_prev(EditorState *state)
{
    if (!state->search->active || state->search->query[0] == '\0')
    {
        editor_set_status(state, "No active search. Use Ctrl+F first.");
        return;
    }
    SearchResult r = search_find_prev(state->search, state->buf);
    _jump_to_match(state, r);
}

void editor_do_replace(EditorState *state)
{
    char query[SEARCH_MAX_QUERY_LEN];
    char replacement[SEARCH_MAX_QUERY_LEN];

    if (!terminal_prompt(state, "Replace: ", query, SEARCH_MAX_QUERY_LEN))
    {
        editor_set_status(state, "Replace cancelled.");
        return;
    }

    if (!terminal_prompt(state, "With: ", replacement, SEARCH_MAX_QUERY_LEN))
    {
        editor_set_status(state, "Replace cancelled.");
        return;
    }

    search_set_query(state->search, query,
                     state->cursor_row, state->cursor_col);

    SearchResult r = search_find_next(state->search, state->buf);
    if (!r.found)
    {
        editor_set_status(state, "Not found: %s", query);
        return;
    }

    _jump_to_match(state, r);

    {
        int  qlen = (int)strlen(query);
        int  rlen = (int)strlen(replacement);
        int  row  = r.row;
        int  col  = r.col;
        int  i;

        const char *line = buffer_get_line(state->buf, row);

        for (i = 0; i < qlen; i++)
        {
            if (col + i < buffer_get_line_length(state->buf, row))
                undo_push_delete_char(state->undo, row, col,
                                      line[col + i]);
        }

        for (i = 0; i < rlen; i++)
            undo_push_insert_char(state->undo, row, col + i, replacement[i]);

        search_replace_current(state->search, state->buf, replacement);

        state->cursor_col    = col + rlen;
        state->preferred_col = state->cursor_col;

        editor_set_status(state, "Replaced. Ctrl+N for next match.");
    }
}

void editor_do_goto_line(EditorState *state)
{
    char input[32];

    if (!terminal_prompt(state, "Go to line: ", input, sizeof(input)))
    {
        editor_set_status(state, "Cancelled.");
        return;
    }

    int target = 0;
    int i;
    for (i = 0; input[i] >= '0' && input[i] <= '9'; i++)
        target = target * 10 + (input[i] - '0');

    target--;

    if (target < 0)
        target = 0;
    if (target >= buffer_get_line_count(state->buf))
        target = buffer_get_line_count(state->buf) - 1;

    state->cursor_row    = target;
    state->cursor_col    = 0;
    state->preferred_col = 0;

    editor_scroll_to_cursor(state);
    editor_set_status(state, "Line %d", target + 1);
}

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
            state->cursor_col    = 0;
            state->preferred_col = 0;
            editor_scroll_to_cursor(state);
            break;

        case KEY_END:
            {
                int len              = buffer_get_line_length(state->buf,
                                                               state->cursor_row);
                state->cursor_col    = len;
                state->preferred_col = len;
                editor_scroll_to_cursor(state);
            }
            break;

        case KEY_PAGE_UP:
            {
                int jump = state->term_rows - 2;
                if (jump < 1) jump = 1;
                state->cursor_row -= jump;
                if (state->cursor_row < 0) state->cursor_row = 0;
                {
                    int len = buffer_get_line_length(state->buf, state->cursor_row);
                    if (state->cursor_col > len) state->cursor_col = len;
                }
                editor_scroll_to_cursor(state);
            }
            break;

        case KEY_PAGE_DOWN:
            {
                int jump     = state->term_rows - 2;
                int max_row  = buffer_get_line_count(state->buf) - 1;
                if (jump < 1) jump = 1;
                state->cursor_row += jump;
                if (state->cursor_row > max_row) state->cursor_row = max_row;
                {
                    int len = buffer_get_line_length(state->buf, state->cursor_row);
                    if (state->cursor_col > len) state->cursor_col = len;
                }
                editor_scroll_to_cursor(state);
            }
            break;

        case KEY_CTRL:
            switch (key.value)
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
                default: break;
            }
            break;

        case KEY_ESCAPE:
            search_clear_highlight(state->search);
            state->search->active = 0;
            break;

        default:
            break;
    }
}

void editor_run(EditorState *state)
{
    terminal_render(state);

    while (state->running)
    {
        KeyEvent key = terminal_read_key();

        if (terminal_was_resized())
        {
            terminal_get_size(&state->term_rows, &state->term_cols);
            editor_scroll_to_cursor(state);
        }

        if (state->status_msg_dirty)
        {
            state->status_msg[0]    = '\0';
            state->status_msg_dirty = 0;
        }

        editor_handle_key(state, key);

        terminal_render(state);
    }
}
