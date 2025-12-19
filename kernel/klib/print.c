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

static const char *get_string(int level) {
	switch (level) {
	case LOG_DEBUG:
		return "["COLOR_BLUE  "debug"COLOR_RESET"]";
	case LOG_INFO:
		return "["COLOR_YELLOW"infos"COLOR_RESET"]";
	case LOG_STATUS:
		return "["COLOR_YELLOW"status"COLOR_RESET"]";
	case LOG_WARNING:
		return "["COLOR_YELLOW" warn "COLOR_RESET"]";
	case LOG_ERROR:
		return "["COLOR_RED   "error"COLOR_RESET"]";
	case LOG_FATAL:
		return "["COLOR_RED   "fatal"COLOR_RESET"]";
	default :
		return "["COLOR_BLUE  "?????"COLOR_RESET"]";
	}
}

void __kprint(const char *filename, long line, int level, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	kprintf("%s %s:%ld ", get_string(level), filename, line);
	kvprintf(fmt, args);
	va_end(args);
}

