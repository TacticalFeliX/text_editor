/* tests/test_search.c
 *
 * Unit tests for the search/replace subsystem.
 * Full test bodies implemented in Phase 6.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/buffer.h"
#include "../src/search.h"

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

/* Helper: build a buffer with known content */
static Buffer *make_buffer(const char **lines, int count)
{
    Buffer *buf = buffer_new();
    buffer_replace_line(buf, 0, lines[0]);
    for (int i = 1; i < count; i++)
        buffer_insert_line(buf, i, lines[i]);
    return buf;
}

static void test_search_find_simple(void)
{
    const char *content[] = { "hello world", "foo bar", "hello again" };
    Buffer     *buf = make_buffer(content, 3);
    SearchCtx  *ctx = search_ctx_new();

    search_set_query(ctx, "hello", 0, 0);
    SearchResult r = search_find_next(ctx, buf);

    ASSERT(r.found == 1,  "found 'hello'");
    ASSERT(r.row == 0,    "found on row 0");
    ASSERT(r.col == 0,    "found at col 0");
    ASSERT(r.length == 5, "match length is 5");

    search_ctx_free(ctx);
    buffer_free(buf);
}

static void test_search_find_next_wraps(void)
{
    const char *content[] = { "abc", "def", "abc" };
    Buffer     *buf = make_buffer(content, 3);
    SearchCtx  *ctx = search_ctx_new();

    search_set_query(ctx, "abc", 0, 0);
    SearchResult r1 = search_find_next(ctx, buf);
    ASSERT(r1.found && r1.row == 0, "first match on row 0");

    SearchResult r2 = search_find_next(ctx, buf);
    ASSERT(r2.found && r2.row == 2, "second match on row 2");

    SearchResult r3 = search_find_next(ctx, buf);
    ASSERT(r3.found && r3.row == 0, "wrap-around: third match back on row 0");

    search_ctx_free(ctx);
    buffer_free(buf);
}

static void test_search_find_prev(void)
{
    const char *content[] = { "abc", "def", "abc" };
    Buffer     *buf = make_buffer(content, 3);
    SearchCtx  *ctx = search_ctx_new();

    /* Start searching from the end */
    search_set_query(ctx, "abc", 2, 3);
    SearchResult r1 = search_find_prev(ctx, buf);
    ASSERT(r1.found && r1.row == 0, "find_prev finds row 0 from row 2");

    SearchResult r2 = search_find_prev(ctx, buf);
    ASSERT(r2.found && r2.row == 2, "find_prev wraps to row 2");

    search_ctx_free(ctx);
    buffer_free(buf);
}

static void test_search_no_match(void)
{
    const char *content[] = { "hello", "world" };
    Buffer     *buf = make_buffer(content, 2);
    SearchCtx  *ctx = search_ctx_new();

    search_set_query(ctx, "nothere", 0, 0);
    SearchResult r = search_find_next(ctx, buf);

    ASSERT(r.found == 0, "no match returns found=0");

    search_ctx_free(ctx);
    buffer_free(buf);
}

static void test_search_find_in_line(void)
{
    ASSERT(search_find_in_line("oo", "foobar", 0, 1) == 1,  "find 'oo' at col 1");
    ASSERT(search_find_in_line("bar", "foobar", 0, 1) == 3, "find 'bar' at col 3");
    ASSERT(search_find_in_line("baz", "foobar", 0, 1) == -1,"'baz' not found");
    ASSERT(search_find_in_line("foo", "foobar", 2, 1) == -1,"'foo' not found after col 2");
}

static void test_search_replace(void)
{
    const char *content[] = { "hello world" };
    Buffer     *buf = make_buffer(content, 1);
    SearchCtx  *ctx = search_ctx_new();

    search_set_query(ctx, "world", 0, 0);
    search_find_next(ctx, buf);
    int result = search_replace_current(ctx, buf, "there");

    ASSERT(result == 0,                                      "replace succeeds");
    ASSERT(strcmp(buffer_get_line(buf, 0), "hello there") == 0,
                                                             "content replaced correctly");

    search_ctx_free(ctx);
    buffer_free(buf);
}

int main(void)
{
    printf("=== Search Tests ===\n");

    test_search_find_simple();
    test_search_find_next_wraps();
    test_search_find_prev();
    test_search_no_match();
    test_search_find_in_line();
    test_search_replace();

    printf("\n%d tests run, %d failed.\n", tests_run, tests_failed);
    return tests_failed;
}
