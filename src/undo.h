#ifndef CEDIT_UNDO_H
#define CEDIT_UNDO_H

#include "buffer.h"

typedef enum UndoType
{
    
    UNDO_INSERT_CHAR,

    UNDO_DELETE_CHAR,

    UNDO_SPLIT_LINE,

    UNDO_MERGE_LINES,

    UNDO_INSERT_PAIR,

} UndoType;

typedef struct UndoRecord
{
    UndoType  type;         /* Classifies this record                          */
    int       row;          /* Buffer row where the operation occurred         */
    int       col;          /* Buffer column where the operation occurred      */
    char      ch;           /* Primary character (INSERT_CHAR, DELETE_CHAR,
                               INSERT_PAIR opening bracket)                    */
    char      ch2;          /* Secondary character (INSERT_PAIR closing bracket) */
    int       indent;       /* Indentation added by smart-indent on Enter      */

    char     *line_text;
    int       line_len;

} UndoRecord;

typedef struct UndoNode
{
    UndoRecord       record;
    struct UndoNode *prev;
} UndoNode;

typedef struct UndoStack
{
    UndoNode *undo_top;     /* Top of the undo stack (NULL if empty)    */
    UndoNode *redo_top;     /* Top of the redo stack (NULL if empty)    */
    int       undo_depth;   /* Number of records on the undo stack      */
    int       redo_depth;   /* Number of records on the redo stack      */
    int       max_depth;    /* Max undo history (0 = unlimited)         */
} UndoStack;

UndoStack *undo_stack_new(int max_depth);

void undo_stack_free(UndoStack *stack);

void undo_push_insert_char(UndoStack *stack, int row, int col, char c);

void undo_push_delete_char(UndoStack *stack, int row, int col, char c);

void undo_push_split_line(UndoStack *stack, int row, int col, int indent);

void undo_push_merge_lines(UndoStack *stack, int row, int col,
                           const char *next_line);

void undo_push_insert_pair(UndoStack *stack, int row, int col,
                           char open, char close);

int undo_undo(UndoStack *stack, Buffer *buf, int *out_row, int *out_col);

int undo_redo(UndoStack *stack, Buffer *buf, int *out_row, int *out_col);

void undo_clear_redo(UndoStack *stack);

int undo_can_undo(const UndoStack *stack);

int undo_can_redo(const UndoStack *stack);

#endif