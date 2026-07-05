
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
Purpose:
    _buf_grow_lines will ensure the lines array has room for at least 'needed' entries
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
Purpose:
    to duplicate a string
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

/*--------------------LifeCycle--------------------*/

/*
    Purpose:
        allocate and init an empty buffer
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
}

/*
Purpose:
    release all memoryowned by the buffer
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
}

/*--------------------char ops---------------------*/

/*
Purpose:
    insert a char in the buffer at the correct line and position
Parameters:
    buf: the buffer under operation
    row, col: position to insert charcater
    c: the character to be inserted
Returns:
    0 on success
    -1 on failure
*/
int buffer_insert_char(Buffer *buf, int row, int col, char c)
{
    if (row <0 || row>=buf->line_count){
        return -1;
    }
    if (col<0 || col> buf->line_lengths[row]){
        return -1;
    }

    int old_len=buf->line_lengths[row];
    int new_len=old_len+1;

    /*grow the string by 1  byte*/
    char *new_line = realloc(buf->lines[row], (size_t)(new_len+1));
    if(new_line == NULL){return -1;}

    buf->lines[row]=new_line;

    memmove(new_line + col + 1, new_line + col, (size_t)(old_len - col+ 1));

    new_line[col] = c;
    buf->line_lengths[row] = new_len;
    buf->modified = 1;
    return 0;
}

/*
Purpose:
    delete the char at row, col
Parameters:
    buf: the buffer under operation
    row, col: from where to delete
Returns:
    0 on success
    -1 on failure
*/
int buffer_delete_char(Buffer *buf, int row, int col)
{
    if(row<0 || row>=buf->line_count){
        return -1;
    }
    if(col<0 || col>buf->line_lengths[row]){
        return -1;
    }

    char *line = buf->lines[row];
    int old_len = buf->line_lengths[row]; 

    memmove(line + col, line + col + 1, (size_t)(old_len - col));

    buf->line_lengths[row] = old_len -1;
    buf->modified = 1;

    return 0;
}

/*---------------------line ops---------------------*/

/*
Purpose:
    split line 'row' at column 'col' (Enter key)
Parameters:
    buf
    row,col
Returns:
    0, -1
*/
int buffer_split_line(Buffer *buf, int row, int col)
{
    if(row< 0 || row>=buf->line_count){
        return -1;
    }
    if(col<0 || col> buf->line_lengths[row]){
        return -1;
    }

    const char *src_line = buf->lines[row];
    int src_len = buf->line_lengths[row];
    int new_len = src_len - col;

    char *new_line = _strdup_len(src_line + col, new_len);
    if(new_line == NULL){
        return -1;
    }
    if(_buf_grow_lines(buf, buf->line_count+1)==-1)
    {
        free(new_line);
        return -1;
    }

    int lines_to_shift = buf->line_count - row - 1;
    if(lines_to_shift>0){
        memmove(buf->lines + row + 2, buf->lines + row + 1, (size_t)lines_to_shift* sizeof(char*));
        memmove(buf->line_lengths + row + 2, buf->line_lengths + row + 1, (size_t)lines_to_shift* sizeof(int*));   
    }

    buf->lines[row+1] = new_line;
    buf->line_lengths[row+1]=new_len;
    buf->line_count++;

    buf->lines[row][col]='\0';
    buf->line_lengths[row]=col;

    buf->modified=1;
    return 0;

}

/*
Purpose:
    merge line 'row+1' onto the end of 'row' (Backspace)
Parameters:
    buf
    row
Returns:
    0,-1
*/
int buffer_merge_lines(Buffer *buf, int row)
{
    if(row<0 || row>= buf->line_count -1){
        return -1;
    }

    int len0 = buf->line_lengths[row];
    int len1 = buf->line_lengths[row+1];
    int merged = len0 + len1;

    char *new_line = realloc(buf->lines[row], (size_t)(merged+1));

    if(new_line==NULL){
        return -1;
    }
    buf->lines[row]=new_line;
    memcpy(new_line+len0, buf->lines[row+1], (size_t)(len1+1));

    buf->line_lengths[row] = merged;

    free(buf->lines[row+1]);

    int lines_to_shift = buf->line_count - row - 2;
    if(lines_to_shift > 0){
        memmove(buf->lines + row + 1, buf->lines + row + 2, (size_t)lines_to_shift * sizeof(char*));
        memmove(buf->line_lengths + row + 1, buf->line_lengths + row + 2, (size_t)lines_to_shift * sizeof(int));
    }

    buf->line_count--;
    buf->modified=1;
    return 0;
}

/*
Purpose:
    insert a new line at position row 
Parameters:
    buf
    row
    text
Returns:
    0,-1
*/
int buffer_insert_line(Buffer *buf, int row, const char *text)
{
    if (row <0 || row >buf->line_count){
        return -1;
    }
    if (text ==NULL){
        text = "";
    }

    int new_len = (int)strlen(text);

    char *new_line = _strdup_len(text, new_len);
    if (new_line ==NULL){
        return -1;
    }

    if (_buf_grow_lines(buf, buf->line_count + 1) == -1){
        free(new_line);
        return -1;
    }
    
    int lines_to_shift = buf->line_count - row;
    if (lines_to_shift > 0)
    {
        memmove(buf->lines + row + 1, buf->lines + row, (size_t)lines_to_shift * sizeof(char *));
        memmove(buf->line_lengths + row + 1, buf->line_lengths + row, (size_t)lines_to_shift * sizeof(int));
    }

    buf->lines[row] = new_line;
    buf->line_lengths[row] = new_len;
    buf->line_count++;
    buf->modified = 1;

    return 0;
}

/*
Purpose:
    removes the line at position 'row'
Parameters:
    buf
    row
Returns:
    0,-1
*/
int buffer_delete_line(Buffer *buf, int row)
{
    if (row< 0 || row>= buf->line_count){
        return -1;
    }

    if (buf->line_count == 1){

        char *empty = _strdup_len("", 0);
        if (empty ==NULL){
            return -1;
        }

        free(buf->lines[0]);
        buf->lines[0]        = empty;
        buf->line_lengths[0] = 0;
        buf->modified        = 1;
        return 0;
    }

    free(buf->lines[row]);

    
    int lines_to_shift = buf->line_count - row - 1;
    if (lines_to_shift > 0){
        memmove(buf->lines + row, buf->lines + row + 1, (size_t)lines_to_shift * sizeof(char *));
        memmove(buf->line_lengths + row, buf->line_lengths + row + 1, (size_t)lines_to_shift * sizeof(int));
    }
    
    buf->line_count--;
    buf->modified = 1;
    return 0;
}

/*-------------------------query ops---------------------------*/

/*
Purpose:
    get the line at row 'row'
Parameters:
    buf
    row
Returns:
    returns a read only pointer to text of line row
*/
const char *buffer_get_line(Buffer *buf, int row)
{
    if (row < 0 || row >= buf->line_count){
        return "";
    }
    return buf->lines[row];
}

/*
Purpose:
    get length of line 'row'
Parameters:
    buf
    row
Returns:
    returns cached length of line row
*/
int buffer_get_line_length(Buffer *buf, int row)
{
    if (row < 0 || row >= buf->line_count){
        return -1;
    }
    return buf->line_lengths[row];
}

/*
Purpose:
    get line count
Parameters:
    buf
Returns:
    returns count
*/
int buffer_get_line_count(Buffer *buf)
{
    return buf->line_count;
}

/*
Purpose:
    is the buffer modified or not
Parameters:
    buf
Returns:
    0/1
*/
int buffer_is_modified(Buffer *buf)
{
    return buf->modified;
}

//state control

/*
Purpose:
    set/clear mod tag
Parameters:
    buf
    value to set
Returns:
    none
*/
void buffer_set_modified(Buffer *buf, int value)
{
    buf->modified = value;
}

/*
Purpose:
    removes all lines and reset to single empty line
Parameters:
    buf
Returns:
    none
*/
void buffer_clear(Buffer *buf)
{
    int i;
    for(i = 0;i<buf->line_count;i++){
        free(buf->lines[i]);
    }

    buf->lines[0]= _strdup_len("",0);
    buf->line_lengths[0]=0;

    buf->line_count=1;
    buf->modified = 0;
}

/*
Purpose:
    replace line at 'row' with text
Parameters:
    buf
    row
    text
Returns:
    0,-1
*/
int buffer_replace_line(Buffer *buf, int row, const char *text)
{
    if (row < 0 || row >= buf->line_count){
        return -1;
    }
    if (text == NULL){
        text = "";
    }

    int   new_len  = (int)strlen(text);
    char *new_line = _strdup_len(text, new_len);

    if (new_line == NULL){
        return -1;
    }

    free(buf->lines[row]);
    buf->lines[row]        = new_line;
    buf->line_lengths[row] = new_len;
    buf->modified          = 1;

    return 0;
}