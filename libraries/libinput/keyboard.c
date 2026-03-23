#include <libinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <wchar.h>

static char *skip_blank(char *ptr) {
	while (isblank(*ptr)) ptr++;
	return ptr;
}

static unsigned int parse_keyname(char *ptr, char **end) {
#define CHECK(key) if (!strncmp(ptr, # key, strlen(# key))) {\
	*end = ptr + strlen(# key);\
	return INPUT_KEY_ ## key;\
}

	CHECK(MOUSE_LEFT);
	CHECK(MOUSE_MIDDLE);
	CHECK(MOUSE_RIGHT);
	CHECK(ESC);
	CHECK(TAB);
	CHECK(BACKSPACE);
	CHECK(ENTER);
	CHECK(DELETE);
	CHECK(INERT);
	CHECK(HOME);
	CHECK(END);
	CHECK(ARROW_UP);
	CHECK(ARROW_DOWN);
	CHECK(ARROW_DOWN);
	CHECK(ARROW_LEFT);
	CHECK(ARROW_RIGHT);
	CHECK(PAGE_UP);
	CHECK(PAGE_DOWN);
	CHECK(LSHIFT);
	CHECK(RSHIFT);
	CHECK(LCRTL);
	CHECK(RCRTL);
	CHECK(LALT);
	CHECK(RALT);
	CHECK(ALTGR);
	CHECK(NUM_LOCK);
	CHECK(SCROLL_LOCK);
	CHECK(CAPS_LOCK);
	CHECK(F1);
	CHECK(F2);
	CHECK(F3);
	CHECK(F4);
	CHECK(F5);
	CHECK(F6);
	CHECK(F7);
	CHECK(F8);
	CHECK(F9);
	CHECK(F10);
	CHECK(F11);
	CHECK(F12);
	CHECK(GUI);
	CHECK(VOLUME_UP);
	CHECK(VOLUME_DOWN);

	if (!*ptr) {
		*end = ptr;
		return 0;
	}

	wchar_t wchar;
	int len = mbtowc(&wchar, ptr, strlen(ptr));
	*end = ptr + len;
	return wchar;
}

static unsigned int to_up(unsigned int keycode) {
	if (keycode < 128) return tolower(keycode);
	return keycode;
}

static int parse_layout(const char *name, libinput_layout_t *layout) {
	char path[256];
	snprintf(path, sizeof(path), "/usr/share/layouts/%s", name);
	FILE *file = fopen(path, "r");
	if (!file) return -1;

	char line[1024];
	while (fgets(line, sizeof(line), file)) {
		char *ptr = skip_blank(line);

		// ignore empty lines and comments
		if (!*ptr || *ptr == '\n' || *ptr == '#') continue;

		if (!strncmp(ptr, "include", 7)) {
			ptr += 7;
			ptr = skip_blank(ptr);
			int ret = parse_layout(ptr, layout);
			if (ret < 0) return ret;
		}

		// key the scan code number
		char *end;
		long scancode = strtol(ptr, &end, 0);
		if (end == ptr || scancode < 0 || scancode >= 256) {
			// invalid scancode
			return -1;
		}
		ptr = end;
		
		ptr = skip_blank(ptr);
		layout->mappings[scancode].lower = parse_keyname(ptr, &end);
		if (end == ptr) return -1;
		ptr = end;

		ptr = skip_blank(ptr);
		layout->mappings[scancode].upper = parse_keyname(ptr, &end);
		if (ptr == end) {
			// by default use upper of lower
			layout->mappings[scancode].upper = to_up(layout->mappings[scancode].lower);
		}
		ptr = end;

		ptr = skip_blank(ptr);
		layout->mappings[scancode].alt = parse_keyname(ptr, &end);
		if (ptr == end) {
			// by default use lower
			layout->mappings[scancode].alt = layout->mappings[scancode].lower;
		}
		ptr = end;
		
		ptr = skip_blank(ptr);
		layout->mappings[scancode].upper_alt = parse_keyname(ptr, &end);
		if (ptr == end) {
			// by default use upper of alt
			layout->mappings[scancode].upper_alt = to_up(layout->mappings[scancode].alt);
		}
	}
	if (ferror(file)) return -1;

	return 0;
}

libinput_layout_t *libinput_open_layout(const char *name) {
	libinput_layout_t *layout = malloc(sizeof(libinput_layout_t));
	memset(layout, 0, sizeof(libinput_layout_t));
	int ret = parse_layout(name, layout);
	if (ret < 0) {
		free(layout);
		return NULL;
	} else {
		return layout;
	}
}

void libinput_close_layout(libinput_layout_t *layout) {
	free(layout);
}

libinput_keyboard_t *libinput_open_keyboard(const char *path, int flags) {
	int fd = libinput_open(path, flags);
	if (fd < 0) return NULL;

	char layout_name[INPUT_LAYOUT_SIZE];
	if (libinput_get_layout(fd, layout_name) < 0) {
		libinput_close(fd);
		return NULL;
	}

	libinput_layout_t *layout = libinput_open_layout(layout_name);
	// fallback on default then qwerty
	if (!layout) {
		layout = libinput_open_layout("default");
	}
	if (!layout) {
		layout = libinput_open_layout("qwerty");
	}
	if (!layout) {
		libinput_close(fd);
		return NULL;
	}

	libinput_keyboard_t *keyboard = malloc(sizeof(libinput_keyboard_t));
	keyboard->layout = layout;
	keyboard->fd = fd;
	keyboard->modifier = 0;
	return keyboard;
}

int libinput_close_keyboard(libinput_keyboard_t *keyboard) {
	libinput_close_layout(keyboard->layout);
	libinput_close(keyboard->fd);
	return 0;
}

static void toggle_shift(libinput_keyboard_t *keyboard) {
	if (keyboard->modifier & LIBINPUT_MODIFIER_SHIFT) {
		keyboard->modifier &= ~LIBINPUT_MODIFIER_SHIFT;
	} else {
		keyboard->modifier |= LIBINPUT_MODIFIER_SHIFT;
	}
}

int libinput_get_keyboard_event(libinput_keyboard_t *keyboard, struct input_event *event) {
	int ret = libinput_get_event(keyboard->fd, event);
	if (ret < 0) return ret;

	if (event->ie_type != IE_KEY_EVENT) return 0;

	libinput_layout_mapping_t *mapping = &keyboard->layout->mappings[event->ie_key.scancode];
	unsigned int value;
	if (keyboard->modifier & LIBINPUT_MODIFIER_ALT) {
		if (keyboard->modifier & LIBINPUT_MODIFIER_SHIFT) {
			value = mapping->upper_alt;
		} else {
			value = mapping->alt;
		}
	} else {
		if (keyboard->modifier & LIBINPUT_MODIFIER_SHIFT) {
			value = mapping->upper;
		} else {
			value = mapping->lower;
		}
	}

	if (value) event->ie_key.key = value;

	switch (value) {
	case INPUT_KEY_ALTGR:
		if (event->ie_key.flags & IE_KEY_RELEASE) {
			keyboard->modifier &= ~LIBINPUT_MODIFIER_ALT;
		} else {
			keyboard->modifier |= LIBINPUT_MODIFIER_ALT;
		}
		break;
	case INPUT_KEY_LSHIFT:
	case INPUT_KEY_RSHIFT:
		toggle_shift(keyboard);
		break;
	case INPUT_KEY_CAPS_LOCK:
		if (event->ie_key.flags & IE_KEY_PRESS) {
			toggle_shift(keyboard);
		}
	} 

	return 0;
}
