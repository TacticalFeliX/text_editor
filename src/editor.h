

#ifndef CEDIT_EDITOR_H
#define CEDIT_EDITOR_H

#include "buffer.h"
#include "undo.h"
#include "search.h"
#include "terminal.h"   

/* config details*/

#define EDITOR_TAB_SIZE_DEFAULT     4
#define EDITOR_AUTO_BRACKET_DEFAULT 1
#define EDITOR_SMART_INDENT_DEFAULT 1
#define EDITOR_MAX_UNDO_DEFAULT     0   /* 0 = unlimited */
#define EDITOR_FILENAME_MAX       256
#define EDITOR_STATUS_MSG_MAX     256

/* editor state*/

typedef struct EditorState
{
    
    Buffer     *buf;
    int         cursor_row;
    int         cursor_col;

    int         preferred_col;

    int         scroll_row;
    int         scroll_col;

    int         term_rows;
    int         term_cols;

    char        filename[EDITOR_FILENAME_MAX];

    UndoStack  *undo;

    SearchCtx  *search;

    char        status_msg[EDITOR_STATUS_MSG_MAX];
    int         status_msg_dirty;

    int         tab_size;
    int         auto_bracket;
    int         smart_indent;

    int         running;

} EditorState;

/*---------------------Lifecycle ----------------------------------------- */

EditorState *editor_init(const char *filename);

void editor_run(EditorState *state);

void editor_free(EditorState *state);

/*-------Key handling-----------------------------------------------------------*/

void editor_handle_key(EditorState *state, KeyEvent key);

/*-----Text editing actions-----------------------------------------------*/

void editor_insert_char(EditorState *state, char c);


void editor_insert_newline(EditorState *state);


void editor_do_backspace(EditorState *state);


void editor_do_delete(EditorState *state);

/*------- Cursor movement----------------------------------------------------------- */

void editor_move_cursor(EditorState *state, int drow, int dcol);

void editor_scroll_to_cursor(EditorState *state);

/*----- File operations --------------------------------------------------- */

void editor_do_save(EditorState *state);

void editor_do_save_as(EditorState *state);

void editor_do_open(EditorState *state);

void editor_do_quit(EditorState *state);

/* --------------------------------------- Undo / redo------------------------ */

void editor_do_undo(EditorState *state);

void editor_do_redo(EditorState *state);

/*--------------------------------- Search ------------------------------------- */

void editor_do_find(EditorState *state);

void editor_do_find_next(EditorState *state);

void editor_do_find_prev(EditorState *state);

void editor_do_replace(EditorState *state);

/*---------------------------- Navigation ------------------------------------- */

void editor_do_goto_line(EditorState *state);

/*--------------------------------- Status bar---------------------------------- */

void editor_set_status(EditorState *state, const char *fmt, ...);

#endif /* CEDIT_EDITOR_H */
