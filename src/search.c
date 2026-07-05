#define _POSIX_C_SOURCE 200809L

#include "search.h"
#include "buffer.h"

#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memcpy, strlen, strstr */

SearchCtx *search_ctx_new(void)
{
    SearchCtx *ctx = malloc(sizeof(SearchCtx));
    if (ctx == NULL)
        return NULL;

    ctx->query[0]       = '\0';
    ctx->last_row       = 0;
    ctx->last_col       = 0;
    ctx->active         = 0;
    ctx->highlight_row  = -1;
    ctx->highlight_col  = -1;
    ctx->highlight_len  = 0;

    return ctx;
}

void search_ctx_free(SearchCtx *ctx)
{
    free(ctx);
}

void search_set_query(SearchCtx *ctx, const char *query,
                      int start_row, int start_col)
{
    strncpy(ctx->query, query, SEARCH_MAX_QUERY_LEN - 1);
    ctx->query[SEARCH_MAX_QUERY_LEN - 1] = '\0';

    ctx->last_row = start_row;

    ctx->last_col = start_col - (int)strlen(ctx->query);

    ctx->active   = 1;

    search_clear_highlight(ctx);
}

int search_find_in_line(const char *query, const char *line,
                        int start_col, int forward)
{
    int  line_len   = (int)strlen(line);
    int  query_len  = (int)strlen(query);

    if (query_len == 0)
        return -1;
    if (start_col < 0)
        start_col = 0;

    if (forward)
    {
        if (start_col >= line_len)
            return -1;

        const char *found = strstr(line + start_col, query);
        if (found == NULL)
            return -1;

        return (int)(found - line);
    }
    else
    {
        int max_start = (start_col - 1 < line_len - query_len)
                        ? start_col - 1
                        : line_len - query_len;

        int pos;
        for (pos = max_start; pos >= 0; pos--)
        {
            if (strncmp(line + pos, query, (size_t)query_len) == 0)
                return pos;
        }
        return -1;
    }
}

SearchResult search_find_next(SearchCtx *ctx, const Buffer *buf)
{
    SearchResult result;
    result.found  = 0;
    result.row    = 0;
    result.col    = 0;
    result.length = 0;

    if (ctx->query[0] == '\0' || !ctx->active)
        return result;

    int total_lines = buffer_get_line_count((Buffer *)buf);
    int query_len   = (int)strlen(ctx->query);

    int cur_row = ctx->last_row;
    int cur_col = ctx->last_col + 1;
    int searched = 0;

    while (searched <= total_lines)
    {
        if (cur_row >= total_lines)
        {
            cur_row = 0;
            cur_col = 0;
        }

        const char *line = buffer_get_line((Buffer *)buf, cur_row);
        int col = search_find_in_line(ctx->query, line, cur_col, 1);

        if (col >= 0)
        {
            ctx->last_row = cur_row;
            ctx->last_col = col;

            ctx->highlight_row = cur_row;
            ctx->highlight_col = col;
            ctx->highlight_len = query_len;

            result.found  = 1;
            result.row    = cur_row;
            result.col    = col;
            result.length = query_len;
            return result;
        }

        cur_row++;
        cur_col = 0;
        searched++;
    }

    search_clear_highlight(ctx);
    return result;
}

SearchResult search_find_prev(SearchCtx *ctx, const Buffer *buf)
{
    SearchResult result;
    result.found  = 0;
    result.row    = 0;
    result.col    = 0;
    result.length = 0;

    if (ctx->query[0] == '\0' || !ctx->active)
        return result;

    int total_lines = buffer_get_line_count((Buffer *)buf);
    int query_len   = (int)strlen(ctx->query);

    int cur_row = ctx->last_row;
    int cur_col = ctx->last_col;
    int searched = 0;

    while (searched <= total_lines)
    {
        if (cur_row < 0)
        {
            cur_row = total_lines - 1;
            cur_col = buffer_get_line_length((Buffer *)buf, cur_row) + 1;
        }

        const char *line = buffer_get_line((Buffer *)buf, cur_row);
        int col = search_find_in_line(ctx->query, line, cur_col, 0);

        if (col >= 0)
        {
            ctx->last_row = cur_row;
            ctx->last_col = col;

            ctx->highlight_row = cur_row;
            ctx->highlight_col = col;
            ctx->highlight_len = query_len;

            result.found  = 1;
            result.row    = cur_row;
            result.col    = col;
            result.length = query_len;
            return result;
        }

        cur_row--;
        if (cur_row >= 0)
            cur_col = buffer_get_line_length((Buffer *)buf, cur_row) + 1;
        searched++;
    }

    search_clear_highlight(ctx);
    return result;
}

int search_replace_current(SearchCtx *ctx, Buffer *buf,
                            const char *replacement)
{
    if (!ctx->active || ctx->highlight_row < 0)
        return -1;

    int row         = ctx->highlight_row;
    int col         = ctx->highlight_col;
    int query_len   = (int)strlen(ctx->query);
    int replace_len = (int)strlen(replacement);

    {
        int i;
        for (i = 0; i < query_len; i++)
        {
            if (buffer_delete_char(buf, row, col) == -1)
                return -2;
        }
    }

    {
        int i;
        for (i = 0; i < replace_len; i++)
        {
            if (buffer_insert_char(buf, row, col + i, replacement[i]) == -1)
                return -2;
        }
    }

    ctx->last_col  = col + replace_len;
    ctx->last_row  = row;

    search_clear_highlight(ctx);
    return 0;
}

void search_clear_highlight(SearchCtx *ctx)
{
    ctx->highlight_row = -1;
    ctx->highlight_col = -1;
    ctx->highlight_len = 0;
}
