#ifndef PRINT_H
#define PRINT_H
#include <stdarg.h>
#include <kernel/vfs.h>

int sprintf(char * str,const char *fmt,...);
int vsprintf(char * buf,const char *fmt,va_list args);
int snprintf(char * str,size_t maxlen, const char *fmt,...);
int vsnprintf(char * buf,size_t maxlen, const char *fmt,va_list args);

void kvfprintf(vfs_node *node,const char *fmt,va_list args);
void kfprintf(vfs_node *node,const char *fmt,...);
void kvprintf(const char *fmt,va_list args);
void kprintf(const char *fmt,...);
void kstatus(const char *status);
void __kdebugf(const char *file,int line,const char *fmt,...);
void kinfof(const char *fmt,...);
void kok(void);
void kfail(void);

#define kdebugf(...) __kdebugf(__FILE__,__LINE__,__VA_ARGS__)

//colors

#define COLOR_RESET "\e[0m"
#define COLOR_RED "\e[0;31m"
#define COLOR_GREEN "\e[0;32m"
#define COLOR_YELLOW "\e[0;33m"
#define COLOR_BLUE "\e[0;34m"
#endif