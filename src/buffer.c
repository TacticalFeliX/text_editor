
#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
typedef struct Buffer
{
    char **lines;            Heap-allocated array of line strings             
    int   *line_lengths;     Cached length of each line (avoids strlen calls) 
    int    line_count;       Number of lines currently stored               
    int    line_capacity;    Number of slots currently allocated              
    int    modified;         1 if unsaved changes exist, 0 otherwise          

} Buffer;
 */

/*
Purpose:_buf_grow_lines will ensure the lines array has room for at least 'needed' entries

Parameters:
    buf: the buffer to be grown
    needed: min reqd capacity
Returns:
    0 on success
    -1 on alloc failure

*/
static int _buf_grow_lines(Buffer *buf, int needed)
{
    /*line capacity is already big enough, no need to grow */
    if(buf->line_capacity >= needed){
        return 0;  
    }

    /*if capacity isnt enough, double it like vector in cpp*/
    int new_cap= buf->line_capacity;
    while(new_cap < needed){
        new_cap*=2;
    }

    /*attempt for reallocation of lines and line_lengths*/
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

/*
Purpose: to duplicate a string

Parameters:
    src: the source string, must not be null
    len: no of bytes to copy from src, excluding \0
Returns:
    pointer to duplicated string on success
    NULL on alloc failure
*/
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

/*LifeCycle*/

/*
    Purpose: allocate and init an empty buffer

    Parameters:
        none
    Returns:
    initialises 1 empty line (lines[0]=""), and makes line_count=1
        NULL on alloc failure
*/
Buffer *buffer_new(void)
{
    Buffer *buf = malloc(sizeof(Buffer));
    if (buf == NULL){
        return NULL;
    }

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

/*
Purpose: release all memoryowned by the buffer

Parameters:
    buf: the buffer to be freed
Returns:
    none
*/
void buffer_free(Buffer *buf)
{
    if(buf==NULL){
        return;
    }
    int i;
    for(i=0;i<buf->line_count;i++){
        free(buf->lines[i]);
    }
    free(buf->lines);
    free(buf->line_lengths);
    free(buf);
};

//char ops
/*
Purpose:

Parameters:

Returns:

*/
int buffer_insert_char(Buffer *buf, int row, int col, char c);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_delete_char(Buffer *buf, int row, int col);

//line ops
/*
Purpose:

Parameters:

Returns:

*/
int buffer_split_line(Buffer *buf, int row, int col);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_merge_lines(Buffer *buf, int row);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_insert_line(Buffer *buf, int row, const char *text);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_delete_line(Buffer *buf, int row);

//query ops
/*
Purpose:

Parameters:

Returns:

*/
const char *buffer_get_line(Buffer *buf, int row);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_get_line_length(Buffer *buf, int row);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_get_line_count(Buffer *buf);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_is_modified(Buffer *buf);

//state control

/*
Purpose:

Parameters:

Returns:

*/
void buffer_set_modified(Buffer *buf, int value);

/*
Purpose:

Parameters:

Returns:

*/
void buffer_clear(Buffer *buf);

/*
Purpose:

Parameters:

Returns:

*/
int buffer_replace_line(Buffer *buf, int row, const char *text);