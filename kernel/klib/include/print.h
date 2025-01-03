#ifndef PRINT_H
#define PRINT_H
#include <stdarg.h>

typedef void(*print_func)(const char ch);

void printfunc(print_func func,const char *fmt,va_list);

void kprintf(const char *fmt,...);
void kstatus(const char *status);
void kdebugf(const char *fmt,...);
void kok(void);
void kfail(void);
#endif