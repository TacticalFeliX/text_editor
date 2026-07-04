
#ifndef CEDIT_BUFFER_H
#define CEDIT_BUFFER_H

#define BUFFER_INITIAL_LINE_CAPACITY 64

// type definition

typedef struct Buffer
{
    char **lines; 
    int *line_lengths;
    int line_count;
    int line_capacity;
    int modified;

} Buffer;

//lifecycle

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