#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int _buf_grow_lines(Buffer *buf, int needed)
{
    if(buf->line_capacity >= needed){
        return 0;
    }

    int new_cap= buf->line_capacity;
    while(new_cap < needed){
        new_cap*=2;
    }

    char **new_lines = realloc(buf->lines,
        (size_t)new_cap*sizeof(char*));
    
    if( new_lines==NULL){
        return -1;
    }

    int *new_lengths = realloc(buf->line_lengths,
        (size_t)new_cap*sizeof(int));

    if(new_lengths == NULL){
        buf->lines=new_lines;
        return -1;
    }

    buf->lines         = new_lines;
    buf->line_lengths  = new_lengths;
    buf->line_capacity = new_cap;
    return 0;
}

static char *_strdup_len(const char *src, int len)
{
    char *copy = malloc((size_t)(len+1));
    if(copy==NULL){
        return NULL;
    }
    memcpy(copy, src, (size_t)len);
    copy[len]='\0';
    return copy;
}

//lifecycle

Buffer *buffer_new(void)
{
    Buffer *buf = malloc(sizeof(Buffer));
    if (buf == NULL)
        return NULL;

    buf->line_count    = 0;
    buf->line_capacity = BUFFER_INITIAL_LINE_CAPACITY;
    buf->modified      = 0;

    buf->lines = malloc((size_t)BUFFER_INITIAL_LINE_CAPACITY * sizeof(char *));
    if (buf->lines == NULL)
    {
        free(buf);
        return NULL;
    }

    buf->line_lengths = malloc((size_t)BUFFER_INITIAL_LINE_CAPACITY * sizeof(int));
    if (buf->line_lengths == NULL)
    {
        free(buf->lines);
        free(buf);
        return NULL;
    }

    buf->lines[0] = _strdup_len("", 0);
    if (buf->lines[0] == NULL)
    {
        free(buf->line_lengths);
        free(buf->lines);
        free(buf);
        return NULL;
    }

    buf->line_lengths[0] = 0;
    buf->line_count      = 1;

    return buf;
};

void buffer_free(Buffer *buf);

//char ops

int buffer_insert_char(Buffer *buf, int row, int col, char c);

int buffer_delete_char(Buffer *buf, int row, int col);

//line ops

int buffer_split_line(Buffer *buf, int row, int col);

int buffer_merge_lines(Buffer *buf, int row);

int buffer_insert_line(Buffer *buf, int row, const char *text);

int buffer_delete_line(Buffer *buf, int row);

//query ops

const char *buffer_get_line(Buffer *buf, int row);

int buffer_get_line_length(Buffer *buf, int row);

int buffer_get_line_count(Buffer *buf);

int buffer_is_modified(Buffer *buf);

//state control

void buffer_set_modified(Buffer *buf, int value);

void buffer_clear(Buffer *buf);

int buffer_replace_line(Buffer *buf, int row, const char *text);