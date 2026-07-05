#define _POSIX_C_SOURCE 200809L

#include "fileio.h"
#include "buffer.h"

#include <stdio.h>     /* fopen, fclose, fread, fwrite, feof, ferror, FILE */
#include <stdlib.h>    /* malloc, realloc, free */
#include <string.h>    /* memchr, strlen */
#include <errno.h>     /* errno */

#define READ_CHUNK_SIZE  8192

int fileio_open(Buffer *buf, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        return -1;

    buffer_clear(buf);

    char *chunk = malloc(READ_CHUNK_SIZE);
    if (chunk == NULL)
    {
        fclose(fp);
        return -2;
    }

    int   line_cap = 256;
    int   line_len = 0;
    char *line_buf = malloc((size_t)line_cap);

    if (line_buf == NULL)
    {
        free(chunk);
        fclose(fp);
        return -2;
    }

    int first_line_inserted = 0;

    size_t bytes_read;
    while ((bytes_read = fread(chunk, 1, READ_CHUNK_SIZE, fp)) > 0)
    {
        char  *pos   = chunk;           /* Current position in the chunk */
        char  *end   = chunk + bytes_read;
        char  *newline;

        while (pos < end)
        {
            newline = memchr(pos, '\n', (size_t)(end - pos));

            if (newline == NULL)
            {
                int remaining = (int)(end - pos);

                if (line_len + remaining >= line_cap)
                {
                    int new_cap = line_cap;
                    while (new_cap <= line_len + remaining)
                        new_cap *= 2;

                    char *new_buf = realloc(line_buf, (size_t)new_cap);
                    if (new_buf == NULL)
                    {
                        free(line_buf);
                        free(chunk);
                        fclose(fp);
                        return -2;
                    }
                    line_buf = new_buf;
                    line_cap = new_cap;
                }

                memcpy(line_buf + line_len, pos, (size_t)remaining);
                line_len += remaining;
                break;
            }

            {
                int seg_len = (int)(newline - pos);

                if (line_len + seg_len + 1 >= line_cap)
                {
                    int new_cap = line_cap;
                    while (new_cap <= line_len + seg_len)
                        new_cap *= 2;

                    char *new_buf = realloc(line_buf, (size_t)new_cap);
                    if (new_buf == NULL)
                    {
                        free(line_buf);
                        free(chunk);
                        fclose(fp);
                        return -2;
                    }
                    line_buf = new_buf;
                    line_cap = new_cap;
                }

                memcpy(line_buf + line_len, pos, (size_t)seg_len);
                line_len += seg_len;

                if (line_len > 0 && line_buf[line_len - 1] == '\r')
                    line_len--;

                line_buf[line_len] = '\0';

                if (!first_line_inserted)
                {
                    buffer_replace_line(buf, 0, line_buf);
                    first_line_inserted = 1;
                }
                else
                {
                    int insert_at = buffer_get_line_count(buf);
                    if (buffer_insert_line(buf, insert_at, line_buf) == -1)
                    {
                        free(line_buf);
                        free(chunk);
                        fclose(fp);
                        return -2;
                    }
                }

                line_len = 0;

                pos = newline + 1;
            }
        }
    }

    if (ferror(fp))
    {
        free(line_buf);
        free(chunk);
        fclose(fp);
        return -1;
    }

    if (line_len > 0)
    {
        line_buf[line_len] = '\0';

        if (!first_line_inserted)
        {
            buffer_replace_line(buf, 0, line_buf);
        }
        else
        {
            int insert_at = buffer_get_line_count(buf);
            buffer_insert_line(buf, insert_at, line_buf);
        }
    }

    free(line_buf);
    free(chunk);
    fclose(fp);

    buffer_set_modified(buf, 0);

    return 0;
}

int fileio_save(Buffer *buf, const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
        return -1;

    int line_count = buffer_get_line_count(buf);
    int i;

    for (i = 0; i < line_count; i++)
    {
        const char *line    = buffer_get_line(buf, i);
        int         line_len = buffer_get_line_length(buf, i);

        /* Write the line content */
        if (line_len > 0)
        {
            if ((int)fwrite(line, 1, (size_t)line_len, fp) != line_len)
            {
                fclose(fp);
                return -1;
            }
        }

        if (fwrite("\n", 1, 1, fp) != 1)
        {
            fclose(fp);
            return -1;
        }
    }

    if (fclose(fp) != 0)
        return -1;

    buffer_set_modified(buf, 0);

    return 0;
}
