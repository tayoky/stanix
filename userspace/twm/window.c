#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <twm.h>
#include "twm-internal.h"

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

    utils_hashmap_add(&windows, window->id, window);
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

window_t *get_window(twm_window_t id) {
    return utils_hashmap_get(&windows, id);
}
