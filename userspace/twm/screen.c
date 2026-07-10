#include <twm-internal.h>
#include <stdlib.h>
#include <string.h>

size_t screens_count;
utils_hashmap_t screens;

static void send_screen_add_event(client_t *client, screen_t *screen) {
	twm_event_screen_t screen_event = {
		.base = {
			.type = TWM_EVENT_SCREEN_ADDED,
			.size = sizeof(screen_event),
		},
		.screen = screen->id,
	};
	send_event(client, (twm_event_t *)&screen_event);
}

static void send_screen_remove_event(client_t *client, screen_t *screen) {
	twm_event_screen_t screen_event = {
		.base = {
			.type = TWM_EVENT_SCREEN_REMOVED,
			.size = sizeof(screen_event),
		},
		.screen = screen->id,
	};
	send_event(client, (twm_event_t *)&screen_event);
}

void screen_add(screen_t *screen) {
	static twm_screen_t id_count = 1;
	screen->id = id_count++;
	screen->invalidate_start_x = LONG_MAX;
	screen->invalidate_start_y = LONG_MAX;
	utils_hashmap_add(&screens, screen->id, screen);
	screens_count++;
	utils_vector_foreach(&screens, client) {
		send_screen_add_event(client, screen);
	}
}

void screen_remove(screen_t *screen) {
	utils_hashmap_remove(&screens,screen->id);
	screens_count--;
	utils_vector_foreach(&screens, client) {
		send_screen_remove_event(client, screen);
	}
	free(screen->name);
	gfx_free(screen->gfx);
	free(screen);
}

void screen_add_fb(const char *path) {
	gfx_t *gfx = gfx_open_framebuffer(path);
	if (!gfx) return;
	
	screen_t *screen = malloc(sizeof(screen_t));
	memset(screen, 0, sizeof(screen_t));
	screen->gfx = gfx;
	screen->name = strdup(path);
	screen_add(screen);
}

void screen_init(void) {
	utils_init_hashmap(&screens);
	for (int i=0; i<10; i++) {
		char path[32];
		sprintf(path, "/dev/fb%d", i);
		screen_add_fb(path);
	}
}

int is_inside_screen(screen_t *screen, long x, long y, long width, long height) {
	if (x + width <= screen->x) return 0;
	if (y + height <= screen->y) return 0;
	if (x >= screen->x + screen->width) return 0;
	if (y >= screen->y + screen->height) return 0;
	return 1;
}

screen_t *get_screen(twm_screen_t id) {
	return utils_hashmap_get(&screens, id);
}

screen_t *get_screen_at(long x, long y) {
	utils_hashmap_foreach(key, element, &screens) {
		(void)id;
		screen_t *screen = element;
		if (x < screen->x) continue;
		if (y < screen->y) continue;
		if (x >= screen->x + screen->gfx->width) continue;
		if (y >= screen->y + screen->gfx->height) continue;
		return screen;
	}
	return NULL;
}

void send_screens(client_t *client) {
	utils_hashmap_foreach(key, screen, &screens) {
		(void)id;
		send_screen_add_event(client, screen);
	}
}
