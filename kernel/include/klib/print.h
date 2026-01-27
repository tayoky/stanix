#ifndef _KERNEL_PRINT_H
#define _KERNEL_PRINT_H

#include <sys/types.h>
#include <stdarg.h>

struct vfs_fd;

void kprint_buf(const char *buf, size_t size);
int sprintf(char *str, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);
int snprintf(char *str, size_t maxlen, const char *fmt, ...);
int vsnprintf(char *buf, size_t maxlen, const char *fmt, va_list args);

void kvfprintf(struct vfs_fd *node, const char *fmt, va_list args);
void kfprintf(struct vfs_fd *node, const char *fmt, ...);
void kvprintf(const char *fmt, va_list args);
void kprintf(const char *fmt, ...);
void kok(void);
void kfail(void);

void __kprint(const char *filename, long line, int level, const char *fmt, ...);

#define kprint(level, ...) __kprint(__FILE__, __LINE__, level, __VA_ARGS__)

#define LOG_DEBUG   1
#define LOG_INFO    2
#define LOG_STATUS  3
#define LOG_WARNING 4
#define LOG_ERROR   5
#define LOG_FATAL   6

// some heplers
#define kdebugf(...)   kprint(LOG_DEBUG  , __VA_ARGS__)
#define kinfof(...)    kprint(LOG_INFO   , __VA_ARGS__)
#define kstatusf(...)  kprint(LOG_STATUS , __VA_ARGS__)
#define kwarningf(...) kprint(LOG_WARNING, __VA_ARGS__)
#define kerrorf(...)   kprint(LOG_ERROR  , __VA_ARGS__)
#define kfatalf(...)   kprint(LOG_FATAL  , __VA_ARGS__)

// colors

#define COLOR_RESET  "\e[0m"
#define COLOR_RED    "\e[0;31m"
#define COLOR_GREEN  "\e[0;32m"
#define COLOR_YELLOW "\e[0;33m"
#define COLOR_BLUE   "\e[0;34m"
#endif
