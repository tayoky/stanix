#ifndef _LIBINPUT_H
#define _LIBINPUT_H

#include <input.h>

typedef struct libinput_layout_mapping {
    unsigned int lower;
    unsigned int upper;
    unsigned int alt;
    unsigned int upper_alt;
} libinput_layout_mapping_t;

typedef struct libinput_layout {
    libinput_layout_mapping_t mappings[256];
} libinput_layout_t;


typedef struct libinput_keyboard {
    libinput_layout_t *layout;
    int fd;
    int modifier;
} libinput_keyboard_t;

#define LIBINPUT_MODIFIER_SHIFT 0x01
#define LIBINPUT_MODIFIER_ALT   0x02

int libinput_open(const char *path, int flags);
int libinput_close(int fd);
int libinput_get_event(int fd, struct input_event *event);
int libinput_get_info(int fd, struct input_info *info);
int libinput_set_layout(int fd, char layout[INPUT_LAYOUT_SIZE]);
int libinput_get_layout(int fd, char layout[INPUT_LAYOUT_SIZE]);
libinput_keyboard_t *libinput_open_keyboard(const char *path, int flags);
int libinput_close_keyboard(libinput_keyboard_t *keyboard);
int libinput_get_keyboard_event(libinput_keyboard_t *keyboard, struct input_event *event);
const char *libinput_class_string(unsigned long class);
const char *libinput_subclass_string(unsigned long class, unsigned long subclass);
libinput_layout_t *libinput_open_layout(const char *name);
void libinput_close_layout(libinput_layout_t *layout);

static inline int libinput_is_graph_key(unsigned long key) {
	return key > 0 && key < INPUT_KEY_FIRST;
}


#endif