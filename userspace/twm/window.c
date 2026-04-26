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
	long x;
	long y;
	real_window_coord(window, &x, &y);
	invalidate_rect(x, y, window->width, window->height);
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

static void remove_window_from_stack(window_t *window) {
	if (window->prev) {
		window->prev->next = window->next;
	} else {
		window_stack_bottom = window->next;
	}
	if (window->next) {
		window->next->prev = window->prev;
	} else {
		window_stack_top = window->prev;
	}
}

void set_window_attr(window_t *window, long attr) {
	// if going from show to hide or from hide to show we need to invalidate
	if ((window->attribute ^ attr) & TWM_ATTR_SHOW) {
		invalidate_window(window);
	}
	window->attribute = attr;
}

window_t *create_window(client_t *client, window_t *parent, long width, long height, const char *title) {
	puts("create window");
	static twm_window_t id_count = 1;
	window_t *window = malloc(sizeof(window_t));
	memset(window, 0, sizeof(window_t));

	window->id     = id_count++;
	window->client = client->id;
	window->width  = width;
	window->height = height;
	window->parent = parent;
	window->attribute = TWM_ATTR_DECORED | TWM_ATTR_SHOW;
	window->title  = strdup(title);
	window->x = (rand() % 100) + 10;	
	window->y = (rand() % 100) + 10;

	// setup a new framebuffer
	char framebuffer_name[64];
	sprintf(framebuffer_name, "/window-%d", window->id);
	window->framebuffer_path = strdup(framebuffer_name);
	int framebuffer_fd = shm_open(framebuffer_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
	size_t framebuffer_size = width * height * (gfx->bpp / 8);
	ftruncate(framebuffer_fd, framebuffer_size);

	// mmap the newly created framebuffer
	window->framebuffer = mmap(NULL, framebuffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, 0);
	close(framebuffer_fd);

	push_window_at_top(window);

	utils_hashmap_add(&windows, window->id, window);
	invalidate_window(window);

	// tell the desktop hook we created a window
	// HACK : but only if the client is notthe hook
	// so it does not messed up two event at once
	if (client->id != desktop_hook) {
		twm_event_desktop_t window_event = {
			.base = {
				.type = TWM_EVENT_DESKTOP,
				.size = sizeof(window_event),
			},
			.type = TWM_WINDOW_CREATED,
			.id = window->id,
		};
		send_event(get_client(desktop_hook), (twm_event_t*)&window_event);
	}

	return window;
}

void destroy_window(window_t *window) {
	if (focus_window == window) {
		focus_window = NULL;
	}
	utils_hashmap_remove(&windows, window->id);
	remove_window_from_stack(window);
	invalidate_window(window);

	size_t framebuffer_size = window->width * window->height * (gfx->bpp / 8);
	munmap(window->framebuffer, framebuffer_size);
	free(window->title);
	shm_unlink(window->framebuffer_path);
	free(window->framebuffer_path);

	
	// tell the desktop hook we destroyed a window
	twm_event_desktop_t window_event = {
		.base = {
			.type = TWM_EVENT_DESKTOP,
			.size = sizeof(window_event),
		},
		.type = TWM_WINDOW_DESTROYED,
		.id = window->id,
	};
	send_event(get_client(desktop_hook), (twm_event_t*)&window_event);
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
	long win_x;
	long win_y;
	real_window_coord(window, &win_x, &win_y);
	if (x + width >= win_x && y + height >= win_y
	&& x < win_x + window->width && y < win_y + window->height) {
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

void real_window_coord(window_t *window, long *x, long *y) {
	*x = 0;
	*y = 0;
	while (window) {
		*x += window->x;
		*y += window->y;
		window = window->parent;
	}
}
