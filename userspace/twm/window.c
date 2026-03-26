#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <twm.h>
#include "twm-internal.h"

utils_hashmap_t windows;
window_t *window_stack_top;
window_t *window_stack_bottom;
window_t *focus_window;

static void invalidate_window(window_t *window) {
	invalidate_rect(window->x - theme.border_width, window->y - 2 * theme.border_width - theme.titlebar_height,
		window->width + 2 * theme.border_width, window->height + 3 * theme.border_width + theme.titlebar_height);
}

void push_window_at_top(window_t *window) {
	if (window->prev || window->next || window_stack_bottom == window) {
		// the window is already in the list we need to remove it first
		if (window->next) {
			window->next->prev = window->prev;
		} else {
			// the window is already on top
			return;
		}
		if (window->prev) {
			window->prev->next = window->next;
		} else {
			window_stack_bottom = window->next;
		}
	}

	// we need to push the window at the end/top of the list
	window->next = NULL;
	window->prev = window_stack_top;
	if (window_stack_top) {
		window_stack_top->next = window;
	} else {
		window_stack_bottom = window;
	}
	window_stack_top = window;
	invalidate_window(window);
}

window_t *create_window(client_t *client, window_t *parent, long width, long height, const char *title) {
	puts("create window");
	static twm_window_t id_count = 0;
	window_t *window = malloc(sizeof(window_t));
	memset(window, 0, sizeof(window_t));

	window->id     = id_count++;
	window->client = client;
	window->width  = width;
	window->height = height;
	window->parent = parent;
	window->x = 100;
	window->y = 100;
	window->title  = strdup(title);
	
	// setup a new framebuffer
	char framebuffer_name[64];
	sprintf(framebuffer_name, "window-%d", window->id);
	window->framebuffer_path = strdup(framebuffer_name);
	int framebuffer_fd = shm_open(framebuffer_name, O_RDWR | O_CREAT | O_TRUNC, 0777);
	size_t framebuffer_size = width * height * (gfx->bpp / 8);
	ftruncate(framebuffer_fd, framebuffer_size);

	// mmap the newly created framebuffer
	window->framebuffer = mmap(NULL, framebuffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, 0);
	close(framebuffer_fd);

	push_window_at_top(window);

	utils_hashmap_add(&windows, window->id, window);
	invalidate_window(window);
	return window;
}

void destroy_window(window_t *window) {
	utils_hashmap_remove(&windows, window->id);

	size_t framebuffer_size = window->width * window->height * (gfx->bpp / 8);
	munmap(window->framebuffer, framebuffer_size);
	free(window->title);
	free(window->framebuffer_path);
	free(window);
}

void move_window(window_t *window, long new_x, long new_y) {
	invalidate_window(window);
	window->x = new_x;
	window->y = new_y;
	invalidate_window(window);
}

window_t *get_window(twm_window_t id) {
	return utils_hashmap_get(&windows, id);
}

int is_inside_window(window_t *window, long x, long y, long width, long height) {
	if (x + width >= window->x - theme.border_width && y + height >= window->y - 2 * theme.border_width - theme.titlebar_height
	&& x < window->x + window->width + theme.border_width && y < window->y + window->height + theme.border_width) {
		return 1;
	}
	return 0;
}

int is_inside_titlebar(window_t *window, long x, long y, long width, long height) {
	if (is_inside_window(window, x, y, width, height) && y < window->y) {
		return 1;
	}
	return 0;
}

window_t *get_window_at(long x, long y) {
	window_t *current = window_stack_top;
	while (current) {
		if (is_inside_window(current, x, y, 0, 0)) {
			return current;
		}
		current = current->prev;
	}
	return NULL;
}

int update_focus(window_t *window) {
	if (window == focus_window) return 0;
	focus_window = window;
	push_window_at_top(window);
	return 1;
}
