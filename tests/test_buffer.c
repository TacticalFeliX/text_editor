/* tests/test_buffer.c
 *
 * Unit tests for the buffer subsystem.
 *
 * IMPLEMENTATION STATUS: Stub — test bodies will be filled in Phase 3
 * alongside the buffer.c implementation.
 *
 * Test framework: hand-rolled (no external dependency).
 * Each test_* function prints PASS or FAIL and returns 0/1.
 * main() counts failures and exits with that count.
 *
 * Run with: make test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/buffer.h"

/* ── Minimal test harness ─────────────────────────────────────────────────── */

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

/* ── Test cases (to be implemented in Phase 3) ────────────────────────────── */

static void test_buffer_new_has_one_empty_line(void)
{
    /* A new buffer must have exactly one empty line */
    Buffer *buf = buffer_new();
    ASSERT(buf != NULL,                     "buffer_new returns non-NULL");
    ASSERT(buffer_get_line_count(buf) == 1, "new buffer has 1 line");
    ASSERT(strcmp(buffer_get_line(buf, 0), "") == 0,
                                            "new buffer line 0 is empty");
    ASSERT(buffer_is_modified(buf) == 0,    "new buffer is not modified");
    buffer_free(buf);
}

static void test_buffer_insert_char_basic(void)
{
    Buffer *buf = buffer_new();
    ASSERT(buffer_insert_char(buf, 0, 0, 'H') == 0, "insert 'H' succeeds");
    ASSERT(buffer_insert_char(buf, 0, 1, 'i') == 0, "insert 'i' succeeds");
    ASSERT(strcmp(buffer_get_line(buf, 0), "Hi") == 0,
                                                       "line content is 'Hi'");
    ASSERT(buffer_get_line_length(buf, 0) == 2,       "line length is 2");
    ASSERT(buffer_is_modified(buf) == 1,               "buffer is modified");
    buffer_free(buf);
}

static void test_buffer_delete_char_basic(void)
{
    Buffer *buf = buffer_new();
    buffer_insert_char(buf, 0, 0, 'A');
    buffer_insert_char(buf, 0, 1, 'B');
    buffer_insert_char(buf, 0, 2, 'C');
    ASSERT(buffer_delete_char(buf, 0, 1) == 0,          "delete 'B' succeeds");
    ASSERT(strcmp(buffer_get_line(buf, 0), "AC") == 0,  "line is 'AC' after delete");
    ASSERT(buffer_get_line_length(buf, 0) == 2,          "length is 2 after delete");
    buffer_free(buf);
}

static void test_buffer_split_line(void)
{
    Buffer *buf = buffer_new();
    buffer_insert_char(buf, 0, 0, 'a');
    buffer_insert_char(buf, 0, 1, 'b');
    buffer_insert_char(buf, 0, 2, 'c');

    ASSERT(buffer_split_line(buf, 0, 2) == 0,              "split succeeds");
    ASSERT(buffer_get_line_count(buf) == 2,                "2 lines after split");
    ASSERT(strcmp(buffer_get_line(buf, 0), "ab") == 0,    "line 0 is 'ab'");
    ASSERT(strcmp(buffer_get_line(buf, 1), "c") == 0,     "line 1 is 'c'");
    buffer_free(buf);
}

static void test_buffer_merge_lines(void)
{
    Buffer *buf = buffer_new();
    buffer_insert_char(buf, 0, 0, 'x');
    buffer_split_line(buf, 0, 1);
    buffer_insert_char(buf, 1, 0, 'y');

    ASSERT(buffer_merge_lines(buf, 0) == 0,               "merge succeeds");
    ASSERT(buffer_get_line_count(buf) == 1,               "1 line after merge");
    ASSERT(strcmp(buffer_get_line(buf, 0), "xy") == 0,   "merged line is 'xy'");
    buffer_free(buf);
}

static void test_buffer_insert_and_delete_line(void)
{
    /*
     * After buffer_new() the buffer is: [ "" ]
     * insert_line(0, "first")  shifts "" to 1: [ "first", "" ]
     * insert_line(1, "second") shifts "" to 2: [ "first", "second", "" ]
     * delete_line(0) removes "first" and shifts up: [ "second", "" ]
     *
     * The test verifies:
     *   (a) Insert shifts existing content correctly.
     *   (b) Delete removes the correct line and shifts up correctly.
     */
    Buffer *buf = buffer_new();
    ASSERT(buffer_insert_line(buf, 0, "first") == 0,  "insert 'first' at row 0");
    ASSERT(buffer_insert_line(buf, 1, "second") == 0, "insert 'second' at row 1");
    ASSERT(buffer_get_line_count(buf) == 3,            "3 lines (incl original empty)");

    /* Verify layout before delete */
    ASSERT(strcmp(buffer_get_line(buf, 0), "first")  == 0, "row 0 is 'first' before delete");
    ASSERT(strcmp(buffer_get_line(buf, 1), "second") == 0, "row 1 is 'second' before delete");
    ASSERT(strcmp(buffer_get_line(buf, 2), "")       == 0, "row 2 is empty before delete");

    /* Delete row 0: "first" is removed; "second" shifts to row 0 */
    ASSERT(buffer_delete_line(buf, 0) == 0,             "delete row 0 succeeds");
    ASSERT(buffer_get_line_count(buf) == 2,             "2 lines after delete");
    ASSERT(strcmp(buffer_get_line(buf, 0), "second") == 0, "row 0 is 'second' after delete");
    ASSERT(strcmp(buffer_get_line(buf, 1), "")       == 0, "row 1 is empty after delete");

    buffer_free(buf);
}

static void test_buffer_clear(void)
{
    Buffer *buf = buffer_new();
    buffer_insert_char(buf, 0, 0, 'X');
    buffer_split_line(buf, 0, 1);
    buffer_clear(buf);
    ASSERT(buffer_get_line_count(buf) == 1,              "1 line after clear");
    ASSERT(strcmp(buffer_get_line(buf, 0), "") == 0,     "line 0 empty after clear");
    buffer_free(buf);
}

static void test_buffer_replace_line(void)
{
    Buffer *buf = buffer_new();
    buffer_insert_char(buf, 0, 0, 'A');
    ASSERT(buffer_replace_line(buf, 0, "replaced") == 0,        "replace succeeds");
    ASSERT(strcmp(buffer_get_line(buf, 0), "replaced") == 0,    "content replaced");
    ASSERT(buffer_get_line_length(buf, 0) == 8,                  "length updated");
    buffer_free(buf);
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== Buffer Tests ===\n");

    test_buffer_new_has_one_empty_line();
    test_buffer_insert_char_basic();
    test_buffer_delete_char_basic();
    test_buffer_split_line();
    test_buffer_merge_lines();
    test_buffer_insert_and_delete_line();
    test_buffer_clear();
    test_buffer_replace_line();

    printf("\n%d tests run, %d failed.\n", tests_run, tests_failed);
    return tests_failed;
}
