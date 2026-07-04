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