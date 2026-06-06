#include <kernel/fbcon.h>
#include <kernel/ini.h>
#include <kernel/print.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/device.h>
#include <kernel/framebuffer.h>

static uint32_t ANSI_color[] = {
	0x000000,
	0xDD0000,
	0x00DD00,
	0xDDDD00,
	0x0000DD,
	0xDD00DD,
	0x00DDDD,
	0xc0c0c0
};

void fbcon_output_char(fbcon_t *fbcon, char c) {
	PSF1_Header *header = fbcon->header;
	char *font_data = fbcon->font;
	if (c == '\n') {
		fbcon->x = 0;
		fbcon->y += header->characterSize + 1;
		// some out of bound check
		if (fbcon->y / header->characterSize + 1 >= fbcon->height) {
			vfs_ioctl(fbcon->framebuffer_dev, IOCTL_FRAMEBUFFER_SCROLL, (void *)(uintptr_t)header->characterSize + 1);
			fbcon->y -= header->characterSize + 1;
		}
		return;
	}

	if (c == '\r') {
		fbcon->x = 0;
		return;
	}

	if (c == '\t') {
		fbcon->x = (fbcon->x / 32 + 2) * 32;
		return;
	}

	if (c == '\e') {
		fbcon->ANSI_esc_mode = 1;
		return;
	}

	if (c == '\b') {
		if (fbcon->x <= 0) {
			return;
		}
		fbcon->x -= 8;
		return;
	}

	if (fbcon->ANSI_esc_mode) {
		if (c == 'm') {
			if (fbcon->ANSI_esc_mode == 3) {
				fbcon->font_color = 0xFFFFFF;
			}
			fbcon->ANSI_esc_mode = 0;
			return;
		}

		if (fbcon->ANSI_esc_mode == 5) {
			c-= '0';
			if ((uintptr_t)c >= sizeof(ANSI_color) / sizeof(uint32_t)) {
				// out of bound
				return;
			}
			fbcon->font_color = ANSI_color[(uint8_t)c];
		}
		fbcon->ANSI_esc_mode++;
		return;
	}

	uintptr_t current_byte = c * header->characterSize;

	// get color
	uint32_t font_color = fbcon->font_color;
	uint32_t back_color = fbcon->back_color;

	for (uint16_t y = 0; y < header->characterSize; y++) {
		for (uint8_t x = 0; x < 8; x++) {
			if ((font_data[current_byte] >> (7 - x)) & 0x01) {
				draw_pixel(fbcon->framebuffer_dev, fbcon->x + x, fbcon->y + y, font_color, &fbcon->fb_info);
			} else {
				draw_pixel(fbcon->framebuffer_dev, fbcon->x + x, fbcon->y + y, back_color, &fbcon->fb_info);
			}
		}

		current_byte++;
	}
	fbcon->x += 8;
}

static ssize_t fbcon_output(tty_t *tty, const char *buf, size_t count) {
	fbcon_t *fbcon = container_of(tty, fbcon_t, tty);
	for (size_t i=0; i < count; i++) {
		fbcon_output_char(fbcon, buf[i]);
	}
	return count;
}

static int fbcon_ioctl(tty_t *tty, long request, void *arg) {
	fbcon_t *fbcon = container_of(tty, fbcon_t, tty);
	(void)fbcon;
	(void)request;
	(void)arg;
	// TODO : add ioctl to change font, ...
	return -ENOSYS;
}

static tty_ops_t fbcon_ops = {
	.ioctl =  fbcon_ioctl,
	.out = fbcon_output,
};

static device_driver_t fbcon_driver = {
	.name = "kernel fbcon",
};

void init_fbcon(void) {
	kstatusf("init terminal fbcon ...");
	//not activated by default
	char *activated_value = ini_get_value(kernel->conf_file, "terminal_emulator", "activate");
	if (!activated_value) {
		kfail();
		kinfof("can't find terminal_emualtor.activate key in conf file");
		return;
	}

	if (strcmp(activated_value, "true")) {
		kok();
		kinfof("terminal_emulator not enable\n");
		kinfof("actviate it by stetting the activate key in the conf file to true\n");
		return;
	}
	kfree(activated_value);

	device_driver_register(&fbcon_driver);

	//now find the framebuffer to use
	char *framebuffer_path = ini_get_value(kernel->conf_file, "terminal_emulator", "framebuffer");
	if (!framebuffer_path) {
		kfail();
		kinfof("can't find an frammebuffer to use in conf file\n");
		return;
	}

	//and open the frammebuffer
	vfs_fd_t *framebuffer_dev = vfs_open(framebuffer_path, O_WRONLY);
	if (IS_ERR(framebuffer_dev)) {
		kfail();
		kinfof("fail to open device : %s\n", framebuffer_path);
		kfree(framebuffer_path);
		return;
	}
	kfree(framebuffer_path);

	char *font_path = ini_get_value(kernel->conf_file, "terminal_emulator", "font");
	if (!font_path) {
		font_path = ini_get_value(kernel->conf_file, "terminal_emulator", "font.path");
	}

	if (!font_path) {
		// can't find the key
		kfail();
		kinfof("can't find font key in conf file\n");
		return;
	}

	// now open the file
	vfs_fd_t *font_file = vfs_open(font_path, O_RDONLY);
	if (IS_ERR(font_file)) {
		kfail();
		kinfof("fail to open file : %s\n", font_path);
		kfree(font_path);
		return;
	}

	// and read it
	struct stat st;
	vfs_getattr(font_file->inode, &st);
	char *font = kmalloc(st.st_size);
	if (vfs_read(font_file, font, 0, st.st_size) != (ssize_t)st.st_size) {
		kfail();
		kinfof("fail to read from file %s\n", font_path);
		return;
	}

	vfs_close(font_file);

	fbcon_t *fbcon = kmalloc(sizeof(fbcon_t));
	memset(fbcon, 0, sizeof(fbcon_t));
	new_tty(&fbcon->tty);
	fbcon->tty.ops = &fbcon_ops;

	// save the framebuffer context
	fbcon->framebuffer_dev = framebuffer_dev;

	// start by parsing the font
	fbcon->header = font;
	if (((PSF1_Header *)font)->magic != PSF1_FONT_MAGIC) {
		kfail();
		kinfof("font %s is not a psf1 file\n", font_path);
		kfree(font_path);
		return;
	}

	// init width height and the char buffer
	vfs_ioctl(framebuffer_dev, IOCTL_GET_FB_INFO, &fbcon->fb_info);
	fbcon->width = fbcon->fb_info.width / 8;
	fbcon->height = fbcon->fb_info.height / (((PSF1_Header *)font)->characterSize + 1);

	fbcon->font_type = FONT_TYPE_PSF1;
	fbcon->font = font + sizeof(PSF1_Header);

	// init cursor position
	fbcon->x = 0;
	fbcon->y = 0;
	fbcon->ANSI_esc_mode = 0;

	// init default color
	fbcon->font_color = 0xFFFFFF;
	fbcon->back_color = 0x000000;

	// create the device
	fbcon->tty.device.driver = &fbcon_driver;
	if (device_register_fmt(&fbcon->tty.device, "tty%d") < 0) {
		kfail();
		kinfof("terminal emulator init but can't create device /dev/tty0\n");
		return;
	}

	vfs_fd_t *tty = vfs_open("/dev/tty0", O_WRONLY);
	kfprintf(tty, "[" COLOR_YELLOW "infos" COLOR_RESET "] PMM, VMM vfs tmpFS and other aready init \n");
	vfs_close(tty);

	kok();
	kinfof("succefuly load font from file %s\n", font_path);
	kfree(font_path);
}
