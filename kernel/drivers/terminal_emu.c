#include <kernel/terminal_emu.h>
#include <kernel/ini.h>
#include <kernel/print.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/device.h>
#include <kernel/framebuffer.h>

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

void term_draw_char(char c,terminal_emu_t *terminal_dev){
	PSF1_Header *header = terminal_dev->header;
	char * font_data = terminal_dev->font;
	if(c == '\n'){
		terminal_dev->x = 0;
		terminal_dev->y += header->characterSize +1;
		//some out of bound check
		if(terminal_dev->y / header->characterSize + 1 >= terminal_dev->height){
			vfs_ioctl(terminal_dev->framebuffer_dev,IOCTL_FRAMEBUFFER_SCROLL,(void *)(uintptr_t)header->characterSize + 1);
			terminal_dev->y -= header->characterSize + 1;
		}
		return;
	}

	if(c == '\t'){
		terminal_dev->x = (terminal_dev->x / 32 + 2) * 32;
		return ;
	}

	if(c == '\e'){
		terminal_dev->ANSI_esc_mode = 1;
		return;
	}

	if(c == '\b'){
		if(terminal_dev->x <= 0){
			return;
		}
		terminal_dev->x -= 8;
		return ;
	}

	if(terminal_dev->ANSI_esc_mode){
		if(c == 'm'){
			if(terminal_dev->ANSI_esc_mode == 3){
				terminal_dev->font_color = 0xFFFFFF;
			}
			terminal_dev->ANSI_esc_mode = 0;
			return;
		}

		if(terminal_dev->ANSI_esc_mode == 5){
			c-= '0';
			if( (uintptr_t)c >= sizeof(ANSI_color) / sizeof(uint32_t)){
				//out of bound
				return;
			}
			terminal_dev->font_color = ANSI_color[(uint8_t)c];
		}
		terminal_dev->ANSI_esc_mode++;
		return;
	}

	uintptr_t current_byte = c * header->characterSize;

	//get color
	uint32_t font_color = terminal_dev->font_color;
	uint32_t back_color = terminal_dev->back_color;

	for (uint16_t y = 0; y < header->characterSize; y++){
		for (uint8_t x = 0; x < 8; x++){
			if((font_data[current_byte] >> (7 - x)) & 0x01){
				draw_pixel(terminal_dev->framebuffer_dev,terminal_dev->x + x,terminal_dev->y + y,font_color,&terminal_dev->fb_info);
			} else {
				draw_pixel(terminal_dev->framebuffer_dev,terminal_dev->x + x,terminal_dev->y + y,back_color,&terminal_dev->fb_info);
			}
		}
		
		current_byte++;
	}
	terminal_dev->x += 8;
}

int term_ioctl(vfs_fd_t *fd,long request,void *arg){
	//make compiler happy
	(void)arg;
	terminal_emu_t *term = fd->private;
	switch (request)
	{
	case IOCTL_TTY_WIDTH:
		return term->width;
		break;
	case IOCTL_TTY_HEIGHT:
		return term->height;
		break;
	case IOCTL_TTY_CURX:
		return term->x;
		break;
	case IOCTL_TTY_CURY:
		return term->y;
		break;
	default:
		return -1;
		break;
	}
}

ssize_t term_write(vfs_fd_t *fd,const void *vbuffer,off_t offset,size_t count){
	//make compiler happy
	(void)offset;

	//get the option of the terminal
	terminal_emu_t *terminal_dev = fd->private;

	//the buffer is just char
	char *buffer = (char *)vbuffer;

	for (size_t i = 0; i < count; i++){
		term_draw_char(buffer[i],terminal_dev);
	}
	
	return count;
}

vfs_ops_t terminal_ops = {
	.ioctl =  term_ioctl,
	.write = term_write,
};

device_driver_t terminal_emulator = {
	.name = "kernel terminal emulator",
};

void init_terminal_emualtor(void){
	kstatusf("init terminal emulator ...");
	//not activated by default
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

	register_device_driver(&terminal_emulator);

	//now find the framebuffer to use
	char *framebuffer_path = ini_get_value(kernel->conf_file,"terminal_emulator","framebuffer");
	if(!framebuffer_path){
		kfail();
		kinfof("can't find an frammebuffer to use in conf file\n");
		return;
	}

	//and open the frammebuffer
	vfs_fd_t *framebuffer_dev = vfs_open(framebuffer_path,O_WRONLY);
	if(!framebuffer_dev){
		kfail();
		kinfof("fail to open device : %s\n", framebuffer_path);
		kfree(framebuffer_path);
		return;
	}
	kfree(framebuffer_path);

	char *font_path = ini_get_value(kernel->conf_file,"terminal_emulator","font");
	if (!font_path) {
		font_path = ini_get_value(kernel->conf_file,"terminal_emulator","font.path");
	}

	if (!font_path) {
		// can't find the key
		kfail();
		kinfof("can't find font key in conf file\n");
		return;
	}

	// now open the file
	vfs_fd_t *font_file = vfs_open(font_path, O_RDONLY);
	if (!font_file) {
		kfail();
		kinfof("fail to open file : %s\n", font_path);
		kfree(font_path);
		return;
	}

	//and read it
	struct stat st;
	vfs_getattr(font_file->inode,&st);
	char *font = kmalloc(st.st_size);
	if(vfs_read(font_file,font,0,st.st_size) != (ssize_t)st.st_size){
		kfail();
		kinfof("fail to read from file %s\n",font_path);
		return;
	}

	vfs_close(font_file);

	terminal_emu_t *terminal_dev = kmalloc(sizeof(terminal_emu_t));
	memset(terminal_dev, 0, sizeof(terminal_emu_t));

	//save the framebuffer context
	terminal_dev->framebuffer_dev = framebuffer_dev;

	//start parse the font
	terminal_dev->header = font;
	if(((PSF1_Header *)font)->magic != PSF1_FONT_MAGIC){
		kfail();
		kinfof("font %s is not a psf1 file\n",font_path);
		kfree(font_path);
		return;
	}

	//init width height and the char buffer
	vfs_ioctl(framebuffer_dev, IOCTL_GET_FB_INFO, &terminal_dev->fb_info);
	terminal_dev->width = terminal_dev->fb_info.width / 8;
	terminal_dev->height = terminal_dev->fb_info.height / (((PSF1_Header *)font)->characterSize + 1);

	terminal_dev->font_type = FONT_TYPE_PSF1;
	terminal_dev->font = font + sizeof(PSF1_Header);

	//init cursor pos
	terminal_dev->x = 0;
	terminal_dev->y = 0;
	terminal_dev->ANSI_esc_mode = 0;

	//init default color
	terminal_dev->font_color = 0xFFFFFF;
	terminal_dev->back_color = 0x000000;

	//create the device
	terminal_dev->device.type   = DEVICE_CHAR;
	terminal_dev->device.name   = strdup("tty0");
	terminal_dev->device.ops    = &terminal_ops;
	terminal_dev->device.driver = &terminal_emulator;
	if(register_device((device_t*)terminal_dev)){
		kfail();
		kinfof("terminal emulator init but can't create device /dev/tty0\n");
		return;
	}

	vfs_fd_t *tty = vfs_open("/dev/tty0",O_WRONLY);
	kfprintf(tty,"[" COLOR_YELLOW "infos" COLOR_RESET "] PMM, VMM vfs tmpFS and other aready init \n");
	vfs_close(tty);

	kok();
	kinfof("succefuly load font from file %s\n",font_path);
	kfree(font_path);
}
