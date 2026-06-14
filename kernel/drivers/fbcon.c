#include <kernel/device.h>
#include <kernel/fbcon.h>
#include <kernel/framebuffer.h>
#include <kernel/ini.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <ctype.h>

// a lot of code here is taken from libterm, https://github.com/tayoky/libterm

uint32_t ansi_colors[] = {
	0x000000, // black
	0xD00000, // red
	0x00D000, // green
	0xD0D000, // yellow
	0x0050D0, // blue  we put a bit of green otherwise it'ws not readable
	0xD000D0, // magenta
	0x00D0D0, // cyan
	0xD0D0D0, // white
	0x808080, // light black
	0xFF0000, // light red
	0x00FF00, // light green
	0xFFFF00, // light yellow
	0x0050FF, // light blue we put a bit of green for same reason as dark blue
	0xFF00FF, // light magenta
	0x00FFFF, // light cyan
	0xFFFFFF, // light white
};

static void fbcon_handle_c0(fbcon_t *fbcon, char c) {
	switch (c) {
	case '\b': // backspace
		if (fbcon->x > 0) fbcon->x--;
		break;
	case '\r':
		fbcon->x = 0;
		break;
	case '\n':
	case '\v':
	case '\f':
		fbcon->y++;
		fbcon->x = 0;
		// TODO : scrolling
		break;
	case '\t':
		fbcon->x -= fbcon->x % 8;
		fbcon->x += 8;
		break;
	case 0x18: // cancel
	case 0x1a: // substitute
		fbcon->state = FBCON_STATE_GROUND;
		break;
	case '\033': // escape char
		fbcon->state = FBCON_STATE_ESCAPE;
		return;
	}
}

static void fbcon_handle_sgr(fbcon_t *fbcon) {
	for (int i = 0; i < fbcon->params_count; i++) {
		switch (fbcon->params[i]) {
		case -1:
		case 0:
			// reset
			fbcon->back_color = 0x000000;
			fbcon->font_color = 0xffffff;
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			fbcon->font_color = ansi_colors[fbcon->params[i] - 30];
			break;
		case 39:
			fbcon->font_color = 0xffffff;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			fbcon->back_color = ansi_colors[fbcon->params[i] - 40];
			break;
		case 49:
			fbcon->back_color = 0x000000;
			break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
			fbcon->font_color = ansi_colors[fbcon->params[i] - 90 + 8];
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
			fbcon->font_color = ansi_colors[fbcon->params[i] - 100 + 8];
			break;
		}
	}
}

static void fbcon_handle_csi_seq(fbcon_t *fbcon, char c) {
	if (fbcon->params[fbcon->params_count] != -1) fbcon->params_count++;

	switch (c) {
	case 'm':
		fbcon_handle_sgr(fbcon);
		break;
	}
	fbcon->state = FBCON_STATE_GROUND;
}

static void fbcon_handle_csi_param(fbcon_t *fbcon, char c) {
	if ((c >= '<' && c <= '?') || c == ',') {
		fbcon->state = FBCON_STATE_CSI_IGNORE;
	} else if (c >= '0' && c <= '9') {
		if (fbcon->params[fbcon->params_count] == -1) {
			fbcon->params[fbcon->params_count] = c - '0';
		} else {
			fbcon->params[fbcon->params_count] *= 10;
			fbcon->params[fbcon->params_count] += c - '0';
		}
	} else if (c == ';') {
		fbcon->params_count++;
		fbcon->params[fbcon->params_count] = -1;
	} else if (c >= '@' && c <= '~') {
		fbcon_handle_csi_seq(fbcon, c);
	}
}

static void fbcon_handle_csi_entry(fbcon_t *fbcon, char c) {
	if (c == ',') {
		fbcon->state = FBCON_STATE_CSI_IGNORE;
	} else if (c >= '<' && c <= '?') {
		fbcon->state = FBCON_STATE_CSI_PARAM;
	} else if ((c >= '0' && c <= '9') || c == ';') {
		fbcon->state = FBCON_STATE_CSI_PARAM;
		fbcon_handle_csi_param(fbcon, c);
	} else if (c >= '@' && c <= '~') {
		fbcon_handle_csi_seq(fbcon, c);
	}
}

static void fbcon_handle_csi_ignore(fbcon_t *fbcon, char c) {
	if (c >= '@' && c <= 0x7e) {
		fbcon->state = FBCON_STATE_GROUND;
	}
}

static void fbcon_handle_esc(fbcon_t *fbcon, char c) {
	if (c == '[') {
		fbcon->state        = FBCON_STATE_CSI_ENTRY;
		fbcon->params_count = 0;
		fbcon->params[0]    = -1;
	} else {
		fbcon->state = FBCON_STATE_GROUND;
	}
}

static void fbcon_print_char(fbcon_t *fbcon, char c) {
	PSF1_Header *header    = fbcon->header;
	char *font_data        = fbcon->font;
	uintptr_t current_byte = c * header->characterSize;
	for (uint16_t y = 0; y < header->characterSize; y++) {
		for (uint8_t x = 0; x < 8; x++) {
			if ((font_data[current_byte] >> (7 - x)) & 0x01) {
				draw_pixel(fbcon->framebuffer_dev, fbcon->x * 8 + x, fbcon->y * header->characterSize + y, fbcon->font_color, &fbcon->fb_info);
			} else {
				draw_pixel(fbcon->framebuffer_dev, fbcon->x * 8 + x, fbcon->y * header->characterSize + y, fbcon->back_color, &fbcon->fb_info);
			}
		}

		current_byte++;
	}
	fbcon->x++;
}

void fbcon_output_char(fbcon_t *fbcon, char c) {
	if (iscntrl(c)) {
		fbcon_handle_c0(fbcon, c);
		return;
	}

	switch (fbcon->state) {
	case FBCON_STATE_GROUND:
		fbcon_print_char(fbcon, c);
		break;
	case FBCON_STATE_ESCAPE:
		fbcon_handle_esc(fbcon, c);
		break;
	case FBCON_STATE_CSI_ENTRY:
		fbcon_handle_csi_entry(fbcon, c);
		break;
	case FBCON_STATE_CSI_PARAM:
		fbcon_handle_csi_param(fbcon, c);
		break;
	case FBCON_STATE_CSI_IGNORE:
		fbcon_handle_csi_ignore(fbcon, c);
		break;
	}
}

static ssize_t fbcon_output(tty_t *tty, const char *buf, size_t count) {
	fbcon_t *fbcon = container_of(tty, fbcon_t, tty);
	for (size_t i = 0; i < count; i++) {
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
	.ioctl = fbcon_ioctl,
	.out   = fbcon_output,
};

static device_driver_t fbcon_driver = {
	.name = "kernel fbcon",
};

void init_fbcon(void) {
	kstatusf("init terminal fbcon ...");
	// not activated by default
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

	// now find the framebuffer to use
	char *framebuffer_path = ini_get_value(kernel->conf_file, "terminal_emulator", "framebuffer");
	if (!framebuffer_path) {
		kfail();
		kinfof("can't find an frammebuffer to use in conf file\n");
		return;
	}

	// and open the frammebuffer
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
	fbcon->tty.size.ws_col    = fbcon->fb_info.width / 8;
	fbcon->tty.size.ws_row    = fbcon->fb_info.height / (((PSF1_Header *)font)->characterSize + 1);
	fbcon->tty.size.ws_xpixel = fbcon->fb_info.width;
	fbcon->tty.size.ws_ypixel = fbcon->fb_info.height;

	fbcon->font_type = FONT_TYPE_PSF1;
	fbcon->font      = font + sizeof(PSF1_Header);

	// init cursor position
	fbcon->x     = 0;
	fbcon->y     = 0;
	fbcon->state = FBCON_STATE_GROUND;

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
