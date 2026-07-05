#ifndef CEDIT_SEARCH_H
#define CEDIT_SEARCH_H

#include "buffer.h"

#define SEARCH_MAX_QUERY_LEN  256

typedef struct SearchResult
{
    int row;        /* Row of the match                   */
    int col;        /* Column of the first match char     */
    int length;     /* Length of the matched string       */
    int found;      /* 1 if a match was found, 0 if not   */
} SearchResult;

typedef struct SearchCtx
{
    char query[SEARCH_MAX_QUERY_LEN];   /* Current search string                  */
    int  last_row;                       /* Row of the most recent match           */
    int  last_col;                       /* Col of the most recent match           */
    int  active;                         /* 1 if a search is in progress           */

    int  highlight_row;
    int  highlight_col;
    int  highlight_len;

} SearchCtx;

SearchCtx *search_ctx_new(void);

void search_ctx_free(SearchCtx *ctx);

void search_set_query(SearchCtx *ctx, const char *query,
                      int start_row, int start_col);

SearchResult search_find_next(SearchCtx *ctx, const Buffer *buf);

SearchResult search_find_prev(SearchCtx *ctx, const Buffer *buf);

int search_find_in_line(const char *query, const char *line,
                        int start_col, int forward);

int search_replace_current(SearchCtx *ctx, Buffer *buf,
                            const char *replacement);

void search_clear_highlight(SearchCtx *ctx);

#endif
