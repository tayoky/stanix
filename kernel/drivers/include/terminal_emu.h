#ifndef TERMINAL_EMU_H
#define TERMINAL_EMU_H

#include <kernel/vfs.h>
#include <stdint.h>

#define FONT_TYPE_PSF1 0x01

typedef struct {
	void *header;
	void *font;
	char font_type;
	char activate;
	vfs_node *frambuffer_dev;
	uintmax_t x;
	uintmax_t y;
	uint32_t font_color;
	uint32_t back_color;
	uintmax_t width;
	uintmax_t height;
	uintmax_t ANSI_esc_mode;
} terminal_emu_settings;

typedef struct {
    uint16_t magic; // Magic bytes for identification.
    uint8_t fontMode; // PSF font mode.
    uint8_t characterSize; // PSF character size.
} PSF1_Header;

#define PSF1_FONT_MAGIC 0x0436

void init_terminal_emualtor(void);
void term_draw_char(char c,terminal_emu_settings *terminal_settings);

#define IOCTL_TTY_WIDTH  0x89798
#define IOCTL_TTY_HEIGHT 0x89146
#define IOCTL_TTY_CURX   0x89545
#define IOCTL_TTY_CURY   0x89504

#endif