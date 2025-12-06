#include <kernel/print.h>
#include <kernel/serial.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

void output_char(char c,void *extra){
	(void)extra;
	//when kout is not init just output trought serial
	if(!kernel->outs){
		write_serial_char(c);
		return;
	}
	//else output to all context open in kernel.outs
	vfs_node *current = *kernel->outs;
	uintmax_t index = 0;
	while(current){
		//out to it
		vfs_write(current,&c,0,sizeof(char));
		index++;
		current = kernel->outs[index];
	}
}

void kok(void){
	kprintf("[" COLOR_GREEN " OK " COLOR_RESET "]\n");
}

void kfail(void){
	kprintf("[" COLOR_RED "FAIL" COLOR_RESET "]\n");
}

void printfunc(print_func func,const char *fmt,va_list args,void *extra){
	char buf[4096];
	int count = vsnprintf(buf,sizeof(buf),fmt,args);
	if(count > (int)sizeof(buf) - 1)count = sizeof(buf) - 1;
	char *ptr = buf;
	while(count > 0){
		count--;
		func(*ptr++,extra);
	}
}

static void kfputc(char c,vfs_node *node){
	vfs_write(node,&c,0,1);
} 

void kfprintf(vfs_node *node,const char *fmt,...){
	va_list args;
	va_start(args,fmt);
	printfunc((print_func)kfputc,fmt,args,node);
	va_end(args);
}


void kvprintf(const char *fmt,va_list args) {
	printfunc(output_char,fmt,args,NULL);
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
	printfunc(output_char,fmt,args,NULL);
	va_end(args);
}

void kinfof(const char *fmt,...){
	kprintf("["COLOR_YELLOW"infos"COLOR_RESET"] ");
	va_list args;
	va_start(args,fmt);
	printfunc(output_char,fmt,args,NULL);
	va_end(args);
}


void kstatus(const char *status){
	kprintf("["COLOR_YELLOW"status"COLOR_RESET"] ");
	kprintf(status);
}