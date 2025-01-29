#include "terminal_emu.h"
#include "ini.h"
#include "print.h"
#include "kernel.h"
#include "string.h"
#include "vfs.h"
#include "framebuffer.h"

uint32_t ANSI_color[] = {
	0x000000,
	0xDD0000,
	0x00DD00,
	0xDDDD00,
	0x0000DD,
	0xDD00DD,
	0x00DDDD,
	0xc0c0c0
};

void term_draw_char(char c){
	PSF1_Header *header = kernel->terminal_settings.header;
	char * font_data = kernel->terminal_settings.font;
	if(c == '\n'){
		kernel->terminal_settings.x = 0;
		kernel->terminal_settings.y += header->characterSize +1;
		return;
	}

	if(c == '\e'){
		kernel->terminal_settings.ANSI_esc_mode = 1;
		return;
	}

	if(kernel->terminal_settings.ANSI_esc_mode){
		if(c == 'm'){
			if(kernel->terminal_settings.ANSI_esc_mode == 3){
				kernel->terminal_settings.font_color = 0xFFFFFF;
			}
			kernel->terminal_settings.ANSI_esc_mode = 0;
			return;
		}

		if(kernel->terminal_settings.ANSI_esc_mode == 5){
			c-= '0';
			if( (uint64_t)c >= sizeof(ANSI_color) / sizeof(uint32_t)){
				//out of bound
				return;
			}
			kernel->terminal_settings.font_color = ANSI_color[(uint8_t)c];
		}
		kernel->terminal_settings.ANSI_esc_mode++;
		return;
	}

	uint64_t current_byte = c * header->characterSize;

	//get color
	uint32_t font_color = kernel->terminal_settings.font_color;

	for (uint16_t y = 0; y < header->characterSize; y++){
		for (uint8_t x = 0; x < 8; x++){
			if((font_data[current_byte] >> (7 - x)) & 0x01){
				draw_pixel(kernel->terminal_settings.frambuffer_dev,kernel->terminal_settings.x + x,kernel->terminal_settings.y + y,font_color);
			}
		}
		
		current_byte++;
	}
	kernel->terminal_settings.x += 8;
}

int term_ioctl(vfs_node *node,uint64_t request,void *arg){
	//make compiler happy
	(void)arg;
	terminal_emu_settings *inode = node->dev_inode;
	switch (request)
	{
	case IOCTL_TTY_WIDTH:
		return inode->width;
		break;
	case IOCTL_TTY_HEIGHT:
		return inode->height;
		break;
	case IOCTL_TTY_CURX:
		return inode->x;
		break;
	case IOCTL_TTY_CURY:
		return inode->y;
		break;
	default:
		return -1;
		break;
	}
}

int64_t term_write(vfs_node *node,void *vbuffer,uint64_t offset,size_t count){
	//make compiler happy
	(void)node;
	(void)offset;

	//the buffer is just char
	char *buffer = (char *)vbuffer;

	for (size_t i = 0; i < count; i++){
		term_draw_char(buffer[i]);
	}
	
	return count;
}

device_op term_op = {
	.write = term_write,
	.ioctl = term_ioctl,
};

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

	if(strcmp(activated_value,"true")){
		kok();
		kinfof("terminal_emulator not enable\n");
		kinfof("actviate it by stetting the activate key in the conf file to true\n");
		return;
	}
	kfree(activated_value);

	//now find the framebuffer to use
	char *frambuffer_path = ini_get_value(kernel->conf_file,"terminal_emulator","framebuffer");
	if(!frambuffer_path){
		kfail();
		kinfof("can't find an frammebuffer to use in conf file\n");
		return;
	}

	//and open the frammebuffer
	vfs_node *frambuffer_dev = vfs_open(frambuffer_path);
	if(!frambuffer_dev){
		kfail();
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
		kinfof("fail to open file : %s\n",font_path);
		return;
	}

	//and read it
	char *font = kmalloc(font_file->size);
	if(vfs_read(font_file,font,0,font_file->size) != font_file->size){
		kfail();
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

	//init width height and the char buffer
	kernel->terminal_settings.width = vfs_ioctl(frambuffer_dev,IOCTL_FRAMEBUFFER_WIDTH,NULL) / 8;
	kernel->terminal_settings.height = vfs_ioctl(frambuffer_dev,IOCTL_FRAMEBUFFER_HEIGHT,NULL) / ((PSF1_Header *)font)->characterSize;

	kernel->terminal_settings.font_type = FONT_TYPE_PSF1;
	kernel->terminal_settings.font = font + sizeof(PSF1_Header);

	//init cursor pos
	kernel->terminal_settings.x = 0;
	kernel->terminal_settings.y = 0;
	kernel->terminal_settings.ANSI_esc_mode = 0;

	//init default color
	kernel->terminal_settings.font_color = 0xFFFFFF;
	kernel->terminal_settings.back_color = 0x000000;

	kernel->terminal_settings.activate = 1;
	printfunc(term_draw_char,"vfs PMM inird and other thing aready init\n",NULL);

	//create the device
	if(vfs_create_dev("dev:/tty0",&term_op,&kernel->terminal_settings)){
		kfail();
		kinfof("terminal emualotr init but can't create dev dev:/tty0\n");
		return;
	}

	kok();
	kinfof("succefuly load font from file %s\n",font_path);
	kfree(font_path);
}