#ifndef PRINT_H
#define PRINT_H
#include <stdarg.h>
#include <kernel/vfs.h>

typedef void(*print_func)(const char ch,void *extra);

void printfunc(print_func func,const char *fmt,va_list,void *extra);

void kfprintf(vfs_node *node,const char *fmt,...);
int  sprintf(char *buf,const char *fmt,...);
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