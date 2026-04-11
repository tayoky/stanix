#ifndef LIBINI_H
#define LIBINI_H

#include <libutils/shashmap.h>

utils_shashmap_t *ini_parse_file(const char *filename);

#endif
