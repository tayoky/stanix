#include <kernel/device.h>
#include <kernel/fbcon.h>
#include <kernel/framebuffer.h>
#include <kernel/cmdline.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <ctype.h>

// a lot of code here is taken from libterm, https://github.com/stanix-org/libterm

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

static size_t fbcon_pixel_offset(fbcon_t *fbcon, long x, long y) {
	return x * fbcon->fb_info.bpp / 8 + y * fbcon->fb_info.pitch;
}

static void *fbcon_pixel_addr(fbcon_t *fbcon, long x, long y) {
	return fbcon->back_buffer + fbcon_pixel_offset(fbcon, x, y);
}

static void fbcon_draw_pixel(fbcon_t *fbcon, long x, long y, uint32_t color) {
	void *addr = fbcon_pixel_addr(fbcon, x, y);
	switch (fbcon->fb_info.bpp) {
	case 32:
		*(uint32_t*)addr = color;
		break;
	case 24:
		memcpy(addr, &color, sizeof(uint8_t) * 3);
		break;
	}
}

static void fbcon_push_buffer(fbcon_t *fbcon) {
	vfs_write(fbcon->framebuffer_dev, fbcon->back_buffer, 0, fbcon->fb_info.pitch * fbcon->fb_info.height);
}

static void fbcon_push_rect(fbcon_t *fbcon, long x, long y, long width, long height) {
	size_t offset = fbcon_pixel_offset(fbcon, x, y);
	size_t line_size = width * fbcon->fb_info.bpp / 8;
	for (long i = 0; i < height; i++) {
		vfs_write(fbcon->framebuffer_dev, fbcon->back_buffer + offset, offset, line_size);
		offset += fbcon->fb_info.pitch;
	}
}

static void fbcon_linefeed(fbcon_t *fbcon) {
	if (fbcon->y + 1 >= fbcon->fb_info.height / fbcon->font_height) {
		// TODO : scrolling
		memmove(fbcon->back_buffer, fbcon->back_buffer + fbcon->font_height * fbcon->fb_info.pitch, (fbcon->fb_info.height - fbcon->font_height) * fbcon->fb_info.pitch);
		memset(fbcon->back_buffer + (fbcon->fb_info.height - fbcon->font_height) * fbcon->fb_info.pitch, 0, fbcon->font_height * fbcon->fb_info.pitch);
		fbcon_push_buffer(fbcon);
	} else {
		fbcon->y++;
	}
}

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
		fbcon_linefeed(fbcon);
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
	// TODO : support widths other than 8
	char *font_data        = fbcon->font;
	uintptr_t current_byte = c * fbcon->font_width * fbcon->font_height / CHAR_BIT;
	for (uint16_t y = 0; y < fbcon->font_height; y++) {
		for (uint8_t x = 0; x < fbcon->font_width; x++) {
			if ((font_data[current_byte] >> (7 - x)) & 0x01) {
				fbcon_draw_pixel(fbcon, fbcon->x * fbcon->font_width + x, fbcon->y * fbcon->font_height + y, fbcon->font_color);
			} else {
				fbcon_draw_pixel(fbcon, fbcon->x * fbcon->font_width + x, fbcon->y * fbcon->font_height + y, fbcon->back_color);
			}
		}

		current_byte++;
	}
	fbcon_push_rect(fbcon, fbcon->x * fbcon->font_width, fbcon->y * fbcon->font_height, fbcon->font_width, fbcon->font_height);
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

void init_fbcon(void) {
	kstatusf("init terminal fbcon ...");


	// find the framebuffer to use
	const char *framebuffer_name = kcmdline_get_option("--fbcon");
	if (!framebuffer_name) {
		kfail();
		kinfof("cannot find an frammebuffer to use on cmdline\n");
		return;
	}
	
	const char *font_path = kcmdline_get_option("--font");
	if (!font_path) {
		kfail();
		kinfof("cannot find an font to use on cmdline\n");
		return;
	}

	// and open the frammebuffer
	device_t *framebuffer_dev = device_from_name(framebuffer_name);
	if (!framebuffer_dev) {
		kfail();
		kinfof("failed to open device : %s\n", framebuffer_name);
		return;
	}

	// now open the file
	vfs_fd_t *font_file = vfs_open(font_path, O_RDONLY);
	if (IS_ERR(font_file)) {
		kfail();
		kinfof("failed to open file : '%s'\n", font_path);
		device_release(framebuffer_dev);
		return;
	}

	fbcon_t *fbcon = kmalloc(sizeof(fbcon_t));
	memset(fbcon, 0, sizeof(fbcon_t));
	fbcon->tty.ops = &fbcon_ops;

	// open the framebuffer context
	fbcon->framebuffer_dev = device_open(framebuffer_dev, O_WRONLY);
	device_release(framebuffer_dev);

	// start by parsing the font
	PSF1_Header psf1_header;
	if (vfs_read(font_file, &psf1_header, 0, sizeof(psf1_header)) != sizeof(psf1_header)) {
fail_to_read:
		kfail();
		kinfof("fail to read from file %s\n", font_path);
close_font:
		vfs_close(font_file);
free_fbcon:
		kfree(fbcon->back_buffer);
		kfree(fbcon);
		vfs_close(fbcon->framebuffer_dev);
		return;
	}

	if (psf1_header.magic != PSF1_FONT_MAGIC) {
		kfail();
		kinfof("font %s is not a psf1 file\n", font_path);
		goto close_font;
	}
	fbcon->font_width = 8;
	fbcon->font_height = psf1_header.characterSize;

	// and read it
	struct stat st;
	vfs_getattr(font_file->inode, &st);
	fbcon->font_size = st.st_size - sizeof(psf1_header);
	fbcon->font = kmalloc(fbcon->font_size);
	if (vfs_read(font_file, fbcon->font, sizeof(psf1_header), fbcon->font_size) != (ssize_t)fbcon->font_size) {
		goto fail_to_read;
	}
	vfs_close(font_file);

	// init width height and the char buffer
	vfs_ioctl(fbcon->framebuffer_dev, IOCTL_GET_FB_INFO, &fbcon->fb_info);
	fbcon->tty.size.ws_col    = fbcon->fb_info.width / fbcon->font_width;
	fbcon->tty.size.ws_row    = fbcon->fb_info.height / fbcon->font_height;
	fbcon->tty.size.ws_xpixel = fbcon->fb_info.width;
	fbcon->tty.size.ws_ypixel = fbcon->fb_info.height;

	// initalize the backbuffer
	size_t framebuffer_size = fbcon->fb_info.pitch * fbcon->fb_info.height;
	fbcon->back_buffer = kmalloc(framebuffer_size);
	memset(fbcon->back_buffer, 0 , framebuffer_size);

	// init cursor position
	fbcon->x     = 0;
	fbcon->y     = 0;
	fbcon->state = FBCON_STATE_GROUND;

	// init default color
	fbcon->font_color = 0xFFFFFF;
	fbcon->back_color = 0x000000;

	// create the device
	if (tty_register(&fbcon->tty, "tty%d", 0) < 0) {
		kfail();
		kinfof("terminal emulator init but can't create device /dev/tty0\n");
		goto free_fbcon;
	}

	vfs_fd_t *tty = vfs_open("/dev/tty0", O_WRONLY);
	kfprintf(tty, "[" COLOR_YELLOW "infos" COLOR_RESET "] PMM, VMM vfs tmpFS and other aready init \n");
	vfs_close(tty);

	kok();
	kinfof("succefuly load font from file %s\n", font_path);
}
