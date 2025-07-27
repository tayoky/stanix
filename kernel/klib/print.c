#include <kernel/print.h>
#include <kernel/serial.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

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

void printuint(print_func func,uintmax_t integer,uint8_t base,int padding,char paddind_char,void *extra){
	char figures[] = "0123456789ABCDEF";
	char str[64];
	str[63] = '\0';
	memset(str,paddind_char,63);

	if(padding > 63){
		padding = 63;
	}
	
	uintmax_t index = 62;
	while(index > 0){
		char figure = figures[integer%base];

		//put the figure in the string
		str[index] = figure;

		//break if the number as no more figure so we don't end up 
		//with a bunch of zero at the start
		if(integer < base)break;
		integer = integer / base;
		index--;
		padding--;
	}

	//if there are still some padding to do well ... do it !!!
	while(padding > 0){
		padding --;
		index --;
	}

	//now we can actually print the string
	va_list stub;
	printfunc(func,(char *) str + index,stub,extra);
}

#define PAD(n) {int count = n ; while(count > 0)func(pad_char,extra);count--;}

void printfunc(print_func func,const char *fmt,va_list args,void *extra){
	uintmax_t index = 0;
	while (fmt[index]){
		if(fmt[index] == '%'){
			int padding = 0;
			char pad_char = ' ';
			index++;
			switch (fmt[index]){
			case '0':
			case ' ':
				pad_char = fmt[index];
				fmt++;
				break;
			}
			
			if(fmt[index] >= '1' && fmt[index] <= '9'){
				while(fmt[index] >= '0' && fmt[index] <= '9'){
					padding *= 10;
					padding += fmt[index] - '0';
					fmt++;
				}
			}
			//start with string and char because there are the only diferent
			if(fmt[index] == 'c'){
				PAD(padding -1);
				func(va_arg(args,int),extra);
				index++;
				continue;
			}

			if(fmt[index] == 's'){
				char *str = va_arg(args,char *);
				PAD(padding - strlen(str));
				while(*str){
					func(*str,extra);
					str++;
				}
				index ++;
				continue;
			}
			if(fmt[index] == '%'){
				func('%',extra);
				index ++;
				continue;
			}

			//now float
			if(fmt[index] == 'f'){
				//unimplented yet
				PAD(padding);
				index ++;
				continue;
			}

			//now we have only integer left 
			//so we will convert all type in an uintmax_t to make thing easier
			uintmax_t integer;

			switch(fmt[index]){
			case 'l' :
				integer = va_arg(args,long);
				index ++;
				break;
			case 'p' :
				integer = va_arg(args,long);
				break;
			case 'h' :
				integer = va_arg(args,int);
				index ++;
				break;
			default:
				integer = va_arg(args,int);
			}

			//now we need to apply the modfier
			switch(fmt[index]){
			case 'd':
			case 'i':
				//now we we make the abs of the number
				//and put an - if needed so we don't have to care about it anymore
				if(((int64_t)integer) < 0){
					func('-',extra);
					integer = (uintmax_t)(-(int64_t)integer);
				}
				//fallthrough
			case 'u' :
				printuint(func,integer,10,padding,pad_char,extra);
				break;
			case 'p' :
				padding = 15;
				pad_char = '0';
				//fallthrough
			case 'x' :
			case 'X' :
				printuint(func,integer,16,padding,pad_char,extra);
				break;
			case 'o':
				printuint(func,integer,8,padding,pad_char,extra);
				break;
			default :
				kdebugf("invalid identifier '%c'\n",fmt[index]);
			}
			
			index++;
			continue;
		}

		//normal char 
		func(fmt[index],extra);
		index ++;
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

static void kputbuf(char c,char **buf){
	**buf = c;
	(*buf)++;
}

void sprintf(char *buf,const char *fmt,...){
	va_list args;
	va_start(args,fmt);
	printfunc((print_func)kputbuf,fmt,args,&buf);
	*buf = '\0';
	va_end(args);
}

void kprintf(const char *fmt,...){
	va_list args;
	va_start(args,fmt);
	printfunc(output_char,fmt,args,NULL);
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