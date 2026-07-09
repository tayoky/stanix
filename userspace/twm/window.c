#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <twm.h>
#include <unistd.h>
#include "twm-internal.h"

utils_hashmap_t windows;
utils_list_t window_stacks[TWM_ZINDEX_COUNT];
window_t *focus_window;

void window_get_inner_bounds(window_t *window, long *x, long *y, long *width, long *height) {
	*x      = 0;
	*y      = 0;
	*width  = window->width;
	*height = window->height;
	if (window->attribute & TWM_ATTR_MAXIMIZED) {
		*width  = gfx->width;
		*height = gfx->height;
	}
	while (window) {
		if (window->attribute & TWM_ATTR_MAXIMIZED) {
			break;
		}
		*x += window->x;
		*y += window->y;
		window = window->parent;
	}
}

void window_get_bounds(window_t *window, long *x, long *y, long *width, long *height) {
	window_get_inner_bounds(window, x, y, width, height);
	if (window->attribute & TWM_ATTR_DECORED) {
		*x -= theme.border_width;
		*y -= theme.border_width + theme.titlebar_height + theme.border_width;
		*width += 2 * theme.border_width;
		*height += theme.border_width + theme.titlebar_height + 2 * theme.border_width;
	}
}

static void invalidate_window(window_t *window) {
	if (!(window->attribute & TWM_ATTR_SHOW) || (window->attribute & TWM_ATTR_MINIMIZED)) {
		return;
	}
	long x, y, width, height;
	window_get_bounds(window, &x, &y, &width, &height);
	invalidate_rect(x, y, width, height);
}

void push_window_at_top(window_t *window) {
	utils_list_t *stack = &window_stacks[window->zindex];
	if (&window->node == stack->last) {
		// the window is already on top
		return;
	}
	if (utils_list_is_in(stack, &window->node)) {
		// the window is already in the list we need to remove it first
		utils_list_remove(stack, &window->node);
	}

	// we need to push the window at the end/top of the list
	utils_list_append(stack, &window->node);
	invalidate_window(window);
}

static void remove_window_from_stack(window_t *window) {
	utils_list_t *stack = &window_stacks[window->zindex];
	utils_list_remove(stack, &window->node);
}

void set_window_attr(window_t *window, long attr) {
	// if going from show to hide or from hide to show or changing decoring status we need to invalidate
	// switching maxmized/minmized also change it
	if ((window->attribute ^ attr) & (TWM_ATTR_SHOW | TWM_ATTR_DECORED | TWM_ATTR_MAXIMIZED |TWM_ATTR_MINIMIZED)) {
		invalidate_window(window);
	}
	long old_attr = window->attribute;
	window->attribute = attr;
	if ((old_attr ^ attr) & (TWM_ATTR_SHOW | TWM_ATTR_DECORED | TWM_ATTR_MAXIMIZED |TWM_ATTR_MINIMIZED)) {
		invalidate_window(window);
	}

	if ((old_attr ^ attr) & TWM_ATTR_MAXIMIZED) {
		// switch from or to maximized change the size of the framebuffer
		window_mark_framebuffer_old(window);
	}

	if ((old_attr ^ attr) & TWM_ATTR_MAXIMIZED) {
		twm_event_window_t update_event = {
			.base = {
				.type = attr & TWM_ATTR_MAXIMIZED ? TWM_EVENT_WINDOW_MAXIMIZED : TWM_EVENT_WINDOW_UNMAXIMIZED,
				.size = sizeof(update_event),
			},
			.window = window->id,
		};
		send_event_id(window->client, (twm_event_t *)&update_event);
	}

	if ((old_attr ^ attr) & TWM_ATTR_MINIMIZED) {
		twm_event_window_t update_event = {
			.base = {
				.type = attr & TWM_ATTR_MINIMIZED ? TWM_EVENT_WINDOW_MINIMIZED : TWM_EVENT_WINDOW_RESTORED,
				.size = sizeof(update_event),
			},
			.window = window->id,
		};
		send_event_id(window->client, (twm_event_t *)&update_event);
	}
}

window_t *create_window(client_t *client, window_t *parent, long width, long height, const char *title) {
	puts("create window");
	static twm_window_t id_count = 1;
	window_t *window             = malloc(sizeof(window_t));
	memset(window, 0, sizeof(window_t));

	window->id        = id_count++;
	window->client    = client->id;
	window->width     = width;
	window->height    = height;
	window->parent    = parent;
	window->attribute = TWM_ATTR_DECORED | TWM_ATTR_SHOW;
	window->zindex    = parent ? parent->zindex : TWM_ZINDEX_MIDDLE;
	window->title     = strdup(title);
	window->x         = (rand() % 100) + 10;
	window->y         = (rand() % 100) + 10;

	// setup a new framebuffer
	char framebuffer_name[64];
	sprintf(framebuffer_name, "/window-%d", window->id);
	window->framebuffer_path = strdup(framebuffer_name);
	window->framebuffer_fd   = shm_open(framebuffer_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
	window->framebuffer_is_old = 1;

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
			.id   = window->id,
		};
		send_event_id(desktop_hook, (twm_event_t *)&window_event);
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
	
	// unmap if the framebuffer was mapped
	if (window->framebuffer) {
		size_t framebuffer_size = window->fb_info.pitch * window->fb_info.height;
		munmap(window->framebuffer, framebuffer_size);
	}
	close(window->framebuffer_fd);
	shm_unlink(window->framebuffer_path);
	free(window->framebuffer_path);
	free(window->title);


	// tell the desktop hook we destroyed a window
	twm_event_desktop_t window_event = {
		.base = {
			.type = TWM_EVENT_DESKTOP,
			.size = sizeof(window_event),
		},
		.type = TWM_WINDOW_DESTROYED,
		.id   = window->id,
	};
	send_event_id(desktop_hook, (twm_event_t *)&window_event);
	free(window);
}

static void send_update_event(window_t *window) {
	// tell the desktop hook we updated a window
	twm_event_desktop_t window_event = {
		.base = {
			.type = TWM_EVENT_DESKTOP,
			.size = sizeof(window_event),
		},
		.type = TWM_WINDOW_UPDATED,
		.id   = window->id,
	};
	send_event_id(desktop_hook, (twm_event_t *)&window_event);
}

void move_window(window_t *window, long new_x, long new_y) {
	invalidate_window(window);
	window->x = new_x;
	window->y = new_y;
	invalidate_window(window);
}

void window_set_title(window_t *window, const char *title) {
	free(window->title);
	window->title = strdup(title);
	// TODO : maybee don't invalidate the whole window
	invalidate_window(window);
	send_update_event(window);
}

window_t *get_window(twm_window_t id) {
	return utils_hashmap_get(&windows, id);
}

int is_inside_window(window_t *window, long x, long y, long width, long height) {
	if (!(window->attribute & TWM_ATTR_SHOW) || (window->attribute & TWM_ATTR_MINIMIZED)) {
		return 0;
	}
	long win_x, win_y, win_width, win_height;
	window_get_bounds(window, &win_x, &win_y, &win_width, &win_height);
	if (x + width >= win_x && y + height >= win_y
		&& x < win_x + win_width && y < win_y + win_height) {
		return 1;
	}
	return 0;
}

window_t *get_window_at(long x, long y) {
	for (int zindex = TWM_ZINDEX_MIN; zindex <= TWM_ZINDEX_MAX; zindex++) {
		utils_list_reverse_foreach(&window_stacks[zindex], node) {
			window_t *current = WINDOW_FROM_NODE(node);
			if (is_inside_window(current, x, y, 0, 0)) {
				return current;
			}
		}
	}
	return NULL;
}

int update_focus(window_t *window) {
	if (window == focus_window) return 0;
	if (focus_window) {
		twm_event_window_t unfocus_event = {
			.base = {
				.type = TWM_EVENT_WINDOW_UNFOCUS,
				.size = sizeof(unfocus_event),
			},
			.window = focus_window->id,
		};
		send_event_id(focus_window->client, (twm_event_t *)&unfocus_event);
	}
	focus_window = window;

	if (window) {
		push_window_at_top(window);
		twm_event_window_t focus_event = {
			.base = {
				.type = TWM_EVENT_WINDOW_FOCUS,
				.size = sizeof(focus_event),
			},
			.window = window->id,
		};
		send_event_id(window->client, (twm_event_t *)&focus_event);
	}
	return 1;
}

void window_get_fb(window_t *window, twm_fb_info_t *info, const char **framebuffer_path) {
	if (window->framebuffer_is_old) {
		// we need to update the framebuffer, it's out of date

		// unmap if it was already mapped
		if (window->framebuffer) {
			size_t old_framebuffer_size = window->fb_info.pitch * window->fb_info.height;
			munmap(window->framebuffer, old_framebuffer_size);
		}

		window->framebuffer_is_old = 0;

		long win_x, win_y, win_width, win_height;
		window_get_inner_bounds(window, &win_x, &win_y, &win_width, &win_height);

		gfx_t *gfx = window->screen->gfx;
		size_t framebuffer_size  = win_width * win_height * (gfx->bpp / 8);
		ftruncate(window->framebuffer_fd, framebuffer_size);

		window->fb_info.bpp              = gfx->bpp;
		window->fb_info.red_mask_shift   = gfx->red_mask_shift;
		window->fb_info.red_mask_size    = gfx->red_mask_size;
		window->fb_info.green_mask_shift = gfx->green_mask_shift;
		window->fb_info.green_mask_size  = gfx->green_mask_size;
		window->fb_info.blue_mask_shift  = gfx->blue_mask_shift;
		window->fb_info.blue_mask_size   = gfx->blue_mask_size;
		window->fb_info.width            = win_width;
		window->fb_info.height           = win_height;
		window->fb_info.pitch            = win_width * (gfx->bpp / 8);

		// map the newly created fb
		window->framebuffer = mmap(NULL, framebuffer_size, PROT_READ, MAP_SHARED, window->framebuffer_fd, 0);
	}
	*info = window->fb_info;
	*framebuffer_path = window->framebuffer_path;
}

void window_mark_framebuffer_old(window_t *window) {
	window->framebuffer_is_old = 1;
	twm_event_window_t buffer_update_event = {
		.base = {
			.type = TWM_EVENT_WINDOW_BUFFER_UPDATE,
			.size = sizeof(buffer_update_event),
		},
		.window = window->id,
	};
	send_event_id(window->client, (twm_event_t *)&buffer_update_event);
}

void window_set_size(window_t *window, long width, long height) {
	if (!(window->attribute & TWM_ATTR_MAXIMIZED)) {
		invalidate_window(window);
	}
	window->width = width;
	window->height = height;
	
	twm_event_window_t resized_update_event = {
		.base = {
			.type = TWM_EVENT_WINDOW_RESIZED,
			.size = sizeof(resized_update_event),
		},
		.window = window->id,
	};
	send_event_id(window->client, (twm_event_t *)&resized_update_event);

	if (!(window->attribute & TWM_ATTR_MAXIMIZED)) {
		// resizing of course, change the framebuffer size
		window_mark_framebuffer_old(window);
		invalidate_window(window);
	}
}

void window_set_zindex(window_t *window, int zindex) {
	if (window->zindex == zindex) {
		return;
	}
	remove_window_from_stack(window);
	window->zindex = zindex;
	push_window_at_top(window);
}
