#ifndef LIBINI_H
#define LIBINI_H

#include <libutils/shashmap.h>

utils_shashmap_t *ini_parse_file(const char *filename);
void ini_free(utils_shashmap_t *hashmap);

#endif
