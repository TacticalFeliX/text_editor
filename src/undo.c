#define _POSIX_C_SOURCE 200809L

#include "undo.h"
#include "buffer.h"

#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* strdup, strlen, memcpy */
#include <stdio.h>    /* fprintf, stderr */

static void _free_node(UndoNode *node)
{
    if (node == NULL)
        return;
    if (node->record.line_text != NULL)
    {
        free(node->record.line_text);
        node->record.line_text = NULL;
    }
    free(node);
}

static void _free_stack(UndoNode *top)
{
    while (top != NULL)
    {
        UndoNode *prev = top->prev;
        _free_node(top);
        top = prev;
    }
}

static UndoNode *_alloc_node(const UndoRecord *record)
{
    UndoNode *node = malloc(sizeof(UndoNode));
    if (node == NULL)
        return NULL;

    node->record = *record;
    node->prev   = NULL;
    return node;
}

static void _push_onto(UndoNode **top, int *depth, int max_depth,
                       UndoNode *node)
{
    node->prev = *top;
    *top       = node;
    (*depth)++;

    if (max_depth > 0 && *depth > max_depth)
    {
        UndoNode *cur = *top;
        while (cur->prev != NULL && cur->prev->prev != NULL)
            cur = cur->prev;

        if (cur->prev != NULL)
        {
            _free_node(cur->prev);
            cur->prev = NULL;
            (*depth)--;
        }
    }
}

UndoStack *undo_stack_new(int max_depth)
{
    UndoStack *s = malloc(sizeof(UndoStack));
    if (s == NULL)
        return NULL;

    s->undo_top   = NULL;
    s->redo_top   = NULL;
    s->undo_depth = 0;
    s->redo_depth = 0;
    s->max_depth  = max_depth;

    return s;
}

void undo_stack_free(UndoStack *stack)
{
    if (stack == NULL)
        return;

    _free_stack(stack->undo_top);
    _free_stack(stack->redo_top);
    free(stack);
}

void undo_push_insert_char(UndoStack *stack, int row, int col, char c)
{
    undo_clear_redo(stack);

    UndoRecord r;
    r.type      = UNDO_INSERT_CHAR;
    r.row       = row;
    r.col       = col;
    r.ch        = c;
    r.ch2       = 0;
    r.indent    = 0;
    r.line_text = NULL;
    r.line_len  = 0;

    UndoNode *node = _alloc_node(&r);
    if (node == NULL)
        return;

    _push_onto(&stack->undo_top, &stack->undo_depth, stack->max_depth, node);
}

void undo_push_delete_char(UndoStack *stack, int row, int col, char c)
{
    undo_clear_redo(stack);

    UndoRecord r;
    r.type      = UNDO_DELETE_CHAR;
    r.row       = row;
    r.col       = col;
    r.ch        = c;
    r.ch2       = 0;
    r.indent    = 0;
    r.line_text = NULL;
    r.line_len  = 0;

    UndoNode *node = _alloc_node(&r);
    if (node == NULL)
        return;

    _push_onto(&stack->undo_top, &stack->undo_depth, stack->max_depth, node);
}

void undo_push_split_line(UndoStack *stack, int row, int col, int indent)
{
    undo_clear_redo(stack);

    UndoRecord r;
    r.type      = UNDO_SPLIT_LINE;
    r.row       = row;
    r.col       = col;
    r.ch        = 0;
    r.ch2       = 0;
    r.indent    = indent;
    r.line_text = NULL;
    r.line_len  = 0;

    UndoNode *node = _alloc_node(&r);
    if (node == NULL)
        return;

    _push_onto(&stack->undo_top, &stack->undo_depth, stack->max_depth, node);
}

void undo_push_merge_lines(UndoStack *stack, int row, int col,
                           const char *next_line)
{
    undo_clear_redo(stack);

    int   next_len  = (int)strlen(next_line);
    char *saved_text = malloc((size_t)(next_len + 1));
    if (saved_text != NULL)
    {
        memcpy(saved_text, next_line, (size_t)(next_len + 1));
    }

    UndoRecord r;
    r.type      = UNDO_MERGE_LINES;
    r.row       = row;
    r.col       = col;
    r.ch        = 0;
    r.ch2       = 0;
    r.indent    = 0;
    r.line_text = saved_text;
    r.line_len  = (saved_text != NULL) ? next_len : 0;

    UndoNode *node = _alloc_node(&r);
    if (node == NULL)
    {
        free(saved_text);
        return;
    }

    _push_onto(&stack->undo_top, &stack->undo_depth, stack->max_depth, node);
}

void undo_push_insert_pair(UndoStack *stack, int row, int col,
                           char open, char close)
{
    undo_clear_redo(stack);

    UndoRecord r;
    r.type      = UNDO_INSERT_PAIR;
    r.row       = row;
    r.col       = col;
    r.ch        = open;
    r.ch2       = close;
    r.indent    = 0;
    r.line_text = NULL;
    r.line_len  = 0;

    UndoNode *node = _alloc_node(&r);
    if (node == NULL)
        return;

    _push_onto(&stack->undo_top, &stack->undo_depth, stack->max_depth, node);
}

static UndoNode *_build_redo_record(const UndoRecord *undone,
                                     int restored_row, int restored_col)
{
    UndoRecord redo;
    redo.line_text = NULL;
    redo.line_len  = 0;
    redo.indent    = undone->indent;
    redo.ch        = undone->ch;
    redo.ch2       = undone->ch2;
    redo.row       = restored_row;
    redo.col       = restored_col;

    switch (undone->type)
    {
        case UNDO_INSERT_CHAR:
            redo.type = UNDO_INSERT_CHAR;
            redo.row  = undone->row;
            redo.col  = undone->col;
            break;

        case UNDO_DELETE_CHAR:
            redo.type = UNDO_DELETE_CHAR;
            redo.row  = undone->row;
            redo.col  = undone->col;
            break;

        case UNDO_SPLIT_LINE:
            redo.type = UNDO_SPLIT_LINE;
            redo.row  = undone->row;
            redo.col  = undone->col;
            break;

        case UNDO_MERGE_LINES:
            redo.type = UNDO_MERGE_LINES;
            redo.row  = undone->row;
            redo.col  = undone->col;
            if (undone->line_text != NULL)
            {
                redo.line_text = malloc((size_t)(undone->line_len + 1));
                if (redo.line_text != NULL)
                {
                    memcpy(redo.line_text, undone->line_text,
                           (size_t)(undone->line_len + 1));
                    redo.line_len = undone->line_len;
                }
            }
            break;

        case UNDO_INSERT_PAIR:
            redo.type = UNDO_INSERT_PAIR;
            redo.row  = undone->row;
            redo.col  = undone->col;
            break;
    }

    return _alloc_node(&redo);
}

int undo_undo(UndoStack *stack, Buffer *buf, int *out_row, int *out_col)
{
    if (stack->undo_top == NULL)
        return -1;   /* Nothing to undo */

    /* Pop the top node */
    UndoNode *node = stack->undo_top;
    stack->undo_top = node->prev;
    stack->undo_depth--;

    UndoRecord *r        = &node->record;
    int         res_row  = r->row;
    int         res_col  = r->col;

    switch (r->type)
    {
        case UNDO_INSERT_CHAR:
            buffer_delete_char(buf, r->row, r->col);
            res_row = r->row;
            res_col = r->col;
            break;

        case UNDO_DELETE_CHAR:
            buffer_insert_char(buf, r->row, r->col, r->ch);
            res_row = r->row;
            res_col = r->col + 1;
            break;

        case UNDO_SPLIT_LINE:
            /*
             * Remove smart-indentation that was added to line row+1.
             * The first `indent` characters of line row+1 are spaces we added.
             */
            if (r->indent > 0 && r->row + 1 < buffer_get_line_count(buf))
            {
                int i;
                for (i = 0; i < r->indent; i++)
                {
                    /*
                     * Remove the leading space from line row+1.
                     * We always delete at column 0 since each deletion
                     * shifts the content left.
                     */
                    if (buffer_get_line_length(buf, r->row + 1) > 0)
                        buffer_delete_char(buf, r->row + 1, 0);
                }
            }
            buffer_merge_lines(buf, r->row);
            res_row = r->row;
            res_col = r->col;
            break;

        case UNDO_MERGE_LINES:
            buffer_split_line(buf, r->row, r->col);
            res_row = r->row + 1;
            res_col = 0;
            break;

        case UNDO_INSERT_PAIR:
            /* Delete close bracket first (higher col), then open bracket */
            buffer_delete_char(buf, r->row, r->col + 1);
            buffer_delete_char(buf, r->row, r->col);
            res_row = r->row;
            res_col = r->col;
            break;
    }

    /* Build and push the redo record */
    UndoNode *redo_node = _build_redo_record(r, res_row, res_col);
    if (redo_node != NULL)
        _push_onto(&stack->redo_top, &stack->redo_depth, 0, redo_node);

    /* Return cursor position to caller */
    if (out_row != NULL) *out_row = res_row;
    if (out_col != NULL) *out_col = res_col;

    _free_node(node);
    return 0;
}

int undo_redo(UndoStack *stack, Buffer *buf, int *out_row, int *out_col)
{
    if (stack->redo_top == NULL)
        return -1;

    UndoNode *node = stack->redo_top;
    stack->redo_top = node->prev;
    stack->redo_depth--;

    UndoRecord *r       = &node->record;
    int         res_row = r->row;
    int         res_col = r->col;

    switch (r->type)
    {
        case UNDO_INSERT_CHAR:
            buffer_insert_char(buf, r->row, r->col, r->ch);
            undo_push_insert_char(stack, r->row, r->col, r->ch);
            res_row = r->row;
            res_col = r->col + 1;
            break;

        case UNDO_DELETE_CHAR:
            {
                const char *line = buffer_get_line(buf, r->row);
                char deleted_ch  = (line != NULL && r->col < buffer_get_line_length(buf, r->row))
                                   ? line[r->col] : 0;
                buffer_delete_char(buf, r->row, r->col);
                undo_push_delete_char(stack, r->row, r->col, deleted_ch);
            }
            res_row = r->row;
            res_col = r->col;
            break;

        case UNDO_SPLIT_LINE:
            undo_push_split_line(stack, r->row, r->col, r->indent);
            buffer_split_line(buf, r->row, r->col);
            if (r->indent > 0)
            {
                int i;
                for (i = 0; i < r->indent; i++)
                    buffer_insert_char(buf, r->row + 1, i, ' ');
            }
            res_row = r->row + 1;
            res_col = r->indent;
            break;

        case UNDO_MERGE_LINES:
            {
                const char *nxt = buffer_get_line(buf, r->row + 1);
                int         col = buffer_get_line_length(buf, r->row);
                undo_push_merge_lines(stack, r->row, col, nxt ? nxt : "");
                buffer_merge_lines(buf, r->row);
            }
            res_row = r->row;
            res_col = r->col;
            break;

        case UNDO_INSERT_PAIR:
            undo_push_insert_pair(stack, r->row, r->col, r->ch, r->ch2);
            buffer_insert_char(buf, r->row, r->col,     r->ch);
            buffer_insert_char(buf, r->row, r->col + 1, r->ch2);
            res_row = r->row;
            res_col = r->col + 1;
            break;
    }

    if (out_row != NULL) *out_row = res_row;
    if (out_col != NULL) *out_col = res_col;

    _free_node(node);
    return 0;
}

void undo_clear_redo(UndoStack *stack)
{
    _free_stack(stack->redo_top);
    stack->redo_top   = NULL;
    stack->redo_depth = 0;
}

int undo_can_undo(const UndoStack *stack)
{
    return stack->undo_top != NULL;
}

int undo_can_redo(const UndoStack *stack)
{
    return stack->redo_top != NULL;
}
