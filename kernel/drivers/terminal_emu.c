#include "terminal_emu.h"
#include "ini.h"
#include "print.h"
#include "kernel.h"
#include "string.h"
#include "vfs.h"
#include "framebuffer.h"

void term_draw_char(char c){
	PSF1_Header *header = kernel->terminal_settings.header;
	char * font_data = kernel->terminal_settings.font;
	if(c == '\n'){
		kernel->terminal_settings.x = 0;
		kernel->terminal_settings.y += header->characterSize +1;
		return;
	}

	uint64_t current_byte = c * header->characterSize;

	for (uint16_t y = 0; y < header->characterSize; y++){
		for (uint8_t x = 0; x < 8; x++){
			if((font_data[current_byte] >> (7 - x)) & 0x01){
				draw_pixel(kernel->terminal_settings.frambuffer_dev,kernel->terminal_settings.x + x,kernel->terminal_settings.y + y,0xFFFFFF);
			}
		}
		
		current_byte++;
	}
	kernel->terminal_settings.x += 8;
}

void init_terminal_emualtor(void){
	kstatus("init terminal emualtor ...");
	//not activated by default
	kernel->terminal_settings.activate = 0;
	char *activated_value = ini_get_value(kernel->conf_file,"terminal_emulator","activate");
	if(!activated_value){
		kfail();
		kinfof("can't find terminal_emualtor.activate key in conf file");
		return;
	}

	if(!strcmp(activated_value,"true")){
		kernel->terminal_settings.activate = 1;
	}
	kfree(activated_value);

	if(!kernel->terminal_settings.activate){
		kok();
		kinfof("terminal_emulator not enable\n");
		kinfof("actviate it by stetting the activate key in the conf file to true\n");
		return;
	}

	//now find the framebuffer to use
	char *frambuffer_path = ini_get_value(kernel->conf_file,"terminal_emulator","framebuffer");
	if(!frambuffer_path){
		kfail();
		kernel->terminal_settings.activate = 0;
		kinfof("can't find an frammebuffer to use in conf file\n");
		return;
	}

	//and open the frammebuffer
	vfs_node *frambuffer_dev = vfs_open(frambuffer_path);
	if(!frambuffer_dev){
		kfail();
		kernel->terminal_settings.activate = 0;
		kinfof("fail to open device : %s\n",frambuffer_path);
		kfree(frambuffer_path);
		return;
	}
	kernel->terminal_settings.frambuffer_dev = frambuffer_dev;
	kfree(frambuffer_path);

	char *font_path_key = ini_get_value(kernel->conf_file,"terminal_emulator","font");
	if(!font_path_key){
		font_path_key = ini_get_value(kernel->conf_file,"terminal_emulator","font.path");
	}

	if(!font_path_key){
		//can't find the key
		kfail();
		kinfof("can't find font key in conf file\n");
		kernel->terminal_settings.activate = 0;
		return;
	}

	//now find the absolue path
	char *font_path = kmalloc(strlen(font_path_key) + strlen("initrd:/") + 1);
	strcpy(font_path,"initrd:/");
	strcat(font_path,font_path_key);
	kfree(font_path_key);

	//now open the file
	vfs_node *font_file = vfs_open(font_path);
	if(!font_file){
		kfail();
		kernel->terminal_settings.activate = 0;
		kinfof("fail to open file : %s\n",font_path);
		return;
	}

	//and read it
	char *font = kmalloc(font_file->size);
	if(vfs_read(font_file,font,0,font_file->size) != font_file->size){
		kfail();
		kernel->terminal_settings.activate = 0;
		kinfof("fail to read from file %s\n",font_path);
		return;
	}

	//close the file
	vfs_close(font_file);

	//start parse the font
	kernel->terminal_settings.header = font;
	if(((PSF1_Header *)font)->magic != PSF1_FONT_MAGIC){
		kfail();
		kinfof("font %s is not a psf1 file\n",font_path);
		kfree(font_path);
		return;
	}

	kernel->terminal_settings.font_type = FONT_TYPE_PSF1;
	kernel->terminal_settings.font = font + sizeof(PSF1_Header);

	//init cursor pos
	kernel->terminal_settings.x = 0;
	kernel->terminal_settings.y = 0;

	kok();
	kinfof("succefuly load font from file %s\n",font_path);
	kfree(font_path);
	printfunc(term_draw_char,"hello world\n",NULL);
	printfunc(term_draw_char,"line return work !!!\n",NULL);
}