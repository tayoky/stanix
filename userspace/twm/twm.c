#include <twm-internal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <twm.h>
#include <libinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gfx.h>
#include <poll.h>

gfx_t *gfx;
font_t *font;
theme_t theme;
int server_socket;
int kb;
int mouse;
cursor_t cursor;
utils_hashmap_t windows;
utils_vector_t clients;

void error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "twm :");
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
}

void load_theme(void) {
	// TODO : load this from conf file
	theme.titlebar_height = 16;
	theme.border_width = 1;
	theme.font_color = gfx_color(gfx, 0xC0, 0xC0, 0xC0);
	theme.primary = gfx_color(gfx, 0x10, 0x10, 0x10);
	theme.secondary = gfx_color(gfx, 0x10, 0x50, 0x10);
}

void kick_client(client_t *client) {
	// TODO
	close(client->fd);
}

int main() {
	char *path = getenv("TWM");
	if (!path) path = getenv("DISPLAY");
	if (!path) {
		putenv("TWM=/tmp/twm0.sock");
		path = "/tmp/twm0.sock";
	}
	char *kb_path = getenv("KB");
	if (!kb_path) kb_path = "/dev/kb0";
	char *mouse_path = getenv("MOUSE");
	if (!mouse_path) mouse_path = "/dev/mouse0";
	mouse = libinput_open(mouse_path, 0);
	if (mouse < 0) {
		perror(mouse_path);
		return 1;
	}
	kb = libinput_open(kb_path, 0);
	if (kb < 0) {
		perror(kb_path);
		return 1;
	}

	server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (!server_socket) {
		error("failed to create socket");
		return 1;
	}
	struct sockaddr_un unix_path = {
		.sun_family = AF_UNIX,
	};
	strncpy(unix_path.sun_path, path, sizeof(unix_path.sun_path));
	if (bind(server_socket, (struct sockaddr*)&unix_path, sizeof(unix_path)) < 0) {
		error("failed to bind socket");
		return 1;
	}

	if (listen(server_socket, 5) < 0) {
		error("failed to listen on socket");
		return 1;
	}

	pid_t child = fork();
	if (!child) {
		close(server_socket);
		execlp("test-twm", "test-twm", NULL);
		exit(0);
	}

	gfx = gfx_open_framebuffer(NULL);
	if (!gfx) {
		error("no framebuffer");
		return 1;
	}

	font = gfx_load_font(NULL);
	if (!font) {
		error("no font");
		return 1;
	}

	load_theme();


	theme.cursor_texture = gfx_load_texture(gfx, "/usr/share/twm/cursor.qoi");
	if (!font) {
		error("no cursor image");
		return 1;
	}

	utils_init_hashmap(&windows, 512);
	utils_init_vector(&clients, sizeof(client_t));

	init_cursor(&cursor);
	render_and_move_cursor(&cursor, 0, 0);

	for (;;) {
		struct pollfd fds[clients.count + 3];
		for (size_t i=0; i<clients.count; i++) {
			client_t *client = utils_vectot_at(&clients, i);
			fds[i].events = POLLIN | POLLHUP;
			fds[i].fd     = client->fd;
		}
		fds[clients.count].events = POLLIN;
		fds[clients.count].fd = server_socket;
		fds[clients.count + 1].events = POLLIN;
		fds[clients.count + 1].fd = kb;
		fds[clients.count + 2].events = POLLIN;
		fds[clients.count + 2].fd = mouse;
		poll(fds, clients.count+3, 0);
		if (fds[clients.count].revents & POLLIN) {
			int client_fd = accept(server_socket, NULL, NULL);
			if (client_fd < 0) {
				error("fail to accept connection");
				continue;
			}
			puts("client connected");
			client_t client = {
				.fd = client_fd,
			};
			utils_vector_push_back(&clients, &client);
		}
		if (fds[clients.count + 1].revents & POLLIN) {
			handle_keyboard();
		}
		if (fds[clients.count + 2].revents & POLLIN) {
			handle_mouse();
		}
		for (size_t i=0; i<clients.count; i++) {
			client_t *client = utils_vectot_at(&clients, i);
			if (fds[i].revents & POLLIN) {
				handle_request(client);
			}
			if (fds[i].revents & POLLHUP) {
				kick_client(client);
			}
		}
	}

	utils_free_hashmap(&windows);
	utils_free_vector(&clients);

	gfx_free_font(font);
	gfx_free(gfx);
	close(server_socket);
	libinput_close(mouse);
	libinput_close(kb);
	return 0;
}
