#include "print.h"
#include "serial.h"
#include "kernel.h"
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

void printuint(print_func func,uint64_t integer,uint8_t base){
	char figures[] = "0123456789ABCDEF";
	char str[64];
	str[63] = '\0';
	
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
	}

	//now we can actually print the string
	printfunc(func,(char *) str + index,NULL);
}

void printfunc(print_func func,const char *fmt,va_list args){
	uint64_t index = 0;
	while (fmt[index]){
		if(fmt[index] == '%'){
			index++;
			//start with string and char because there are the only diferent
			if(fmt[index] == 'c'){
				func(va_arg(args,int));
				index++;
				continue;
			}

			if(fmt[index] == 's'){
				printfunc(func,va_arg(args,char *),NULL);
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
				index ++;
				continue;
			}

			//now we have only integer left 
			//so we will convert all type in an uint64_t to make thing easier
			uint64_t integer;

			if(fmt[index] == 'l'){
				integer = va_arg(args,long);
				index ++;
			}else if(fmt[index] == 'h'){
				integer = va_arg(args,int);
				index ++;
			}else {
				integer = va_arg(args,int);
			}

			//now we need to apply the modfier
			int8_t modifier = PRINTF_MODIFIER_D;
			if(fmt[index] == 'd' || fmt[index] == 'i'){
				//now we we make the abs of the number
				//and put an - if needed so we don't have to care about it anymore
				if(((int64_t)integer) < 0){
					func('-');
					integer = (uint64_t)(-(int64_t)integer);
				}
			} else if (fmt[index] == 'x'){
				modifier = PRINTF_MODIFIER_X;
			} else if (fmt[index] == 'o'){
				//octal don't even know what it is so...
				//unsuported;
				modifier = PRINTF_MODIFIER_O;
				continue;
			}

			switch (modifier){
				case PRINTF_MODIFIER_D :
					printuint(func,integer,10);
				break;
				case PRINTF_MODIFIER_X :
					printuint(func,integer,16);
				default:
					//we should never end here
					//normally ...
				break;
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