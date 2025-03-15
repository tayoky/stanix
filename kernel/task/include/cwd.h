#ifndef CWD_H
#define CWD_H

/// @brief turn any path into an absolute path
/// @param path the path to turn into an absolute
/// @return the absolute path the caller must free with @ref kfree
char *absolute_path(const char *path);

#endif