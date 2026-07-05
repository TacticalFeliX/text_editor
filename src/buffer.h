
#ifndef CEDIT_BUFFER_H
#define CEDIT_BUFFER_H

#define BUFFER_INITIAL_LINE_CAPACITY 64

// type definition

typedef struct Buffer
{
    char **lines;           /* Heap-allocated array of line strings             */
    int   *line_lengths;    /* Cached length of each line (avoids strlen calls) */
    int    line_count;      /* Number of lines currently stored                 */
    int    line_capacity;   /* Number of slots currently allocated              */
    int    modified;        /* 1 if unsaved changes exist, 0 otherwise          */

} Buffer;

/*
static int _buf_grow_lines(Buffer *buf, int needed){

}
*/

/*
static char *_strdup_len(const char *src, int len){

}
*/

/*lifecycle*/

Buffer *buffer_new(void);

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

#endif 