#include "print.h"
#include "serial.h"
#include "kernel.h"
#include "string.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#define PRINTF_MODIFIER_D 0
#define PRINTF_MODIFIER_X 1
#define PRINTF_MODIFIER_O 2

void output_char(char c){
	//when kout is not init just output trought serial
	if(!kernel->outs){
		write_serial_char(c);
		return;
	}
	//else output to all context open in kernel.outs
	vfs_node *current = *kernel->outs;
	uint64_t index = 0;
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

void printuint(print_func func,uint64_t integer,uint8_t base,int padding,char paddind_char){
	char figures[] = "0123456789ABCDEF";
	char str[64];
	str[63] = '\0';
	memset(str,paddind_char,63);

	if(padding > 63){
		padding = 63;
	}
	
	uint64_t index = 62;
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
	printfunc(func,(char *) str + index,stub);
}

#define PAD(n) {int count = n ; while(count > 0)func(pad_char);count--;}

void printfunc(print_func func,const char *fmt,va_list args){
	uint64_t index = 0;
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
				func(va_arg(args,int));
				index++;
				continue;
			}

			if(fmt[index] == 's'){
				char *str = va_arg(args,char *);
				PAD(padding - strlen(str));
				while(*str){
					func(*str);
					str++;
				}
				index ++;
				continue;
			}
			if(fmt[index] == '%'){
				func('%');
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
			//so we will convert all type in an uint64_t to make thing easier
			uint64_t integer;

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
					func('-');
					integer = (uint64_t)(-(int64_t)integer);
				}
			case 'u' :
				printuint(func,integer,10,padding,pad_char);
				break;
			case 'p' :
				padding = 15;
				pad_char = '0';
			case 'x' :
			case 'X' :
				printuint(func,integer,16,padding,pad_char);
				break;
			case 'o':
				printuint(func,integer,8,padding,pad_char);
				break;
			default :
				kdebugf("invalid identifier '%c'\n",fmt[index]);
			}
			
			index++;
			continue;
		}

		//normal char 
		func(fmt[index]);
		index ++;
	}
}

void kprintf(const char *fmt,...){
	va_list args;
	va_start(args,fmt);
	printfunc(output_char,fmt,args);
	va_end(args);
}

void kdebugf(const char *fmt,...){
	kprintf("["COLOR_BLUE"debug"COLOR_RESET"] ");
	va_list args;
	va_start(args,fmt);
	printfunc(output_char,fmt,args);
	va_end(args);
}

void kinfof(const char *fmt,...){
	kprintf("["COLOR_YELLOW"infos"COLOR_RESET"] ");
	va_list args;
	va_start(args,fmt);
	printfunc(output_char,fmt,args);
	va_end(args);
}


void kstatus(const char *status){
	kprintf("["COLOR_YELLOW"status"COLOR_RESET"] ");
	kprintf(status);
}