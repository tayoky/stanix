#include <kernel/print.h>
#include <kernel/serial.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

void output_buf(const char *buf, size_t size) {
	// when kout is not init just output trought serial
	if(!kernel->outs){
		while (size > 0) {
			write_serial_char(*buf);
			buf++;
			size --;
		}
		return;
	}
	// else output to all context open in kernel.outs
	vfs_node *current = *kernel->outs;
	long index = 0;
	while(current){
		// print to it
		vfs_write(current, buf, 0, size);
		index++;
		current = kernel->outs[index];
	}
}

void kvfprintf(vfs_node *node, const char *fmt, va_list args) {
	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, args);
	vfs_write(node, buf, 0, strlen(buf));
}

void kfprintf(vfs_node *node, const char *fmt, ...) {
	va_list args;
	va_start(args,fmt);
	kvfprintf(node, fmt, args);
	va_end(args);
}

void kvprintf(const char *fmt, va_list args) {
	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, args);
	output_buf(buf, strlen(buf));
}

void kok(void){
	kprintf("[" COLOR_GREEN " OK " COLOR_RESET "]\n");
}

void kfail(void){
	kprintf("[" COLOR_RED "FAIL" COLOR_RESET "]\n");
}

void kprintf(const char *fmt,...){
	va_list args;
	va_start(args,fmt);
	kvprintf(fmt,args);
	va_end(args);
}

void __kdebugf(const char*file,int line,const char *fmt,...){
	kprintf("["COLOR_BLUE"debug"COLOR_RESET"] %s:%d ",file,line);
	va_list args;
	va_start(args,fmt);
	kvprintf(fmt, args);
	va_end(args);
}

void kinfof(const char *fmt,...){
	kprintf("["COLOR_YELLOW"infos"COLOR_RESET"] ");
	va_list args;
	va_start(args,fmt);
	kvprintf(fmt, args);
	va_end(args);
}


void kstatus(const char *status){
	kprintf("["COLOR_YELLOW"status"COLOR_RESET"] ");
	kprintf(status);
}