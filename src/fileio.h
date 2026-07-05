#ifndef CEDIT_FILEIO_H
#define CEDIT_FILEIO_H

#include "buffer.h"

int fileio_open(Buffer *buf, const char *filename);

int fileio_save(Buffer *buf, const char *filename);

#endif