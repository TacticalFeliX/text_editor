/* tests/test_undo.c
 *
 * Unit tests for the undo/redo subsystem.
 * Full test bodies implemented in Phase 5.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/buffer.h"
#include "../src/undo.h"

static int tests_run    = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg)                                                   \
    do {                                                                    \
        tests_run++;                                                        \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
            tests_failed++;                                                 \
        } else {                                                            \
            printf("PASS  %s\n", msg);                                      \
        }                                                                   \
    } while (0)

static void test_undo_stack_starts_empty(void)
{
    UndoStack *s = undo_stack_new(0);
    ASSERT(s != NULL,             "undo_stack_new returns non-NULL");
    ASSERT(!undo_can_undo(s),     "new stack: cannot undo");
    ASSERT(!undo_can_redo(s),     "new stack: cannot redo");
    undo_stack_free(s);
}

static void test_undo_single_char_insert(void)
{
    Buffer    *buf = buffer_new();
    UndoStack *s   = undo_stack_new(0);

    undo_push_insert_char(s, 0, 0, 'A');
    buffer_insert_char(buf, 0, 0, 'A');

    ASSERT(undo_can_undo(s),                          "can undo after insert");
    ASSERT(!undo_can_redo(s),                         "cannot redo before any undo");

    int row = -1, col = -1;
    int result = undo_undo(s, buf, &row, &col);

    ASSERT(result == 0,                               "undo_undo succeeds");
    ASSERT(strcmp(buffer_get_line(buf, 0), "") == 0,  "buffer empty after undo");
    ASSERT(row == 0 && col == 0,                      "cursor restored to (0,0)");
    ASSERT(undo_can_redo(s),                          "can redo after undo");

    undo_stack_free(s);
    buffer_free(buf);
}

static void test_redo_single_char(void)
{
    Buffer    *buf = buffer_new();
    UndoStack *s   = undo_stack_new(0);

    undo_push_insert_char(s, 0, 0, 'B');
    buffer_insert_char(buf, 0, 0, 'B');
    undo_undo(s, buf, NULL, NULL);

    int row = -1, col = -1;
    int result = undo_redo(s, buf, &row, &col);

    ASSERT(result == 0,                               "undo_redo succeeds");
    ASSERT(strcmp(buffer_get_line(buf, 0), "B") == 0, "buffer has 'B' after redo");
    ASSERT(row == 0 && col == 1,                      "cursor at (0,1) after redo");

    undo_stack_free(s);
    buffer_free(buf);
}

static void test_new_edit_clears_redo(void)
{
    Buffer    *buf = buffer_new();
    UndoStack *s   = undo_stack_new(0);

    undo_push_insert_char(s, 0, 0, 'X');
    buffer_insert_char(buf, 0, 0, 'X');
    undo_undo(s, buf, NULL, NULL);

    ASSERT(undo_can_redo(s), "redo available before new edit");

    /* New edit — redo must be cleared */
    undo_push_insert_char(s, 0, 0, 'Y');
    buffer_insert_char(buf, 0, 0, 'Y');

    ASSERT(!undo_can_redo(s), "redo cleared after new edit");

    undo_stack_free(s);
    buffer_free(buf);
}

static void test_undo_split_and_merge(void)
{
    Buffer    *buf = buffer_new();
    UndoStack *s   = undo_stack_new(0);

    buffer_insert_char(buf, 0, 0, 'a');
    buffer_insert_char(buf, 0, 1, 'b');

    undo_push_split_line(s, 0, 1, 0);
    buffer_split_line(buf, 0, 1);

    ASSERT(buffer_get_line_count(buf) == 2, "2 lines after split");

    undo_undo(s, buf, NULL, NULL);
    ASSERT(buffer_get_line_count(buf) == 1,           "1 line after undo split");
    ASSERT(strcmp(buffer_get_line(buf, 0), "ab") == 0, "content restored");

    undo_stack_free(s);
    buffer_free(buf);
}

int main(void)
{
    printf("=== Undo/Redo Tests ===\n");

    test_undo_stack_starts_empty();
    test_undo_single_char_insert();
    test_redo_single_char();
    test_new_edit_clears_redo();
    test_undo_split_and_merge();

    printf("\n%d tests run, %d failed.\n", tests_run, tests_failed);
    return tests_failed;
}
