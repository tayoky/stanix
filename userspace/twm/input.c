#include <twm-internal.h>
#include <libinput.h>

static window_t *grab = NULL;
static long grab_offset_x;
static long grab_offset_y;
int grab_input;

void set_grab(window_t *window, long offset_x, long offset_y) {
	grab = window;
	grab_offset_x = offset_x;
	grab_offset_y = offset_y;
}

static void flush_mouse_move(long rel_x, long rel_y) {
	if (rel_x == 0 && rel_y == 0) return;
	long new_x = cursor.x + rel_x;
	long new_y = cursor.y + rel_y;
	if (new_x < 0) new_x = 0;
	if (new_y < 0) new_y = 0;
	if (new_x + theme.cursor_texture->width > (size_t)gfx->width)  new_x = gfx->width - theme.cursor_texture->width;
	if (new_y + theme.cursor_texture->height > (size_t)gfx->height) new_y = gfx->height - theme.cursor_texture->height;
	move_cursor(&cursor, new_x, new_y);
	if (grab) {
		move_window(grab, new_x + grab_offset_x, new_y + grab_offset_y);
	} else {
		// forward event to the window
		window_t *window;
		if (grab_input) {
			window = focus_window;
		} else {
			window = get_window_at(cursor.x, cursor.y);
		}
		if (!window) return;
		twm_event_input_t twm_event = {
			.base = {
				.size = sizeof(twm_event),
				.type = TWM_EVENT_INPUT,
			},
			.window = window->id,
			.type = TWM_INPUT_MOVE,
			.move = {
				.abs_x = cursor.x - window->x,
				.abs_y = cursor.y - window->y,
				.rex_x = rel_x,
				.rel_y = rel_y,
			},
		};

		send_event(get_client(window->client), (twm_event_t *)&twm_event);
	}
}

// handle input events
void handle_mouse(void) {
	struct input_event event;
	long rel_x = 0;
	long rel_y = 0;
	while (libinput_get_event(mouse, &event) >= 0) {
		if (event.ie_type == IE_MOVE_EVENT) {
			rel_x += event.ie_move.x;
			rel_y += event.ie_move.y;
			continue;
		}
		if (event.ie_type == IE_KEY_EVENT) {
			flush_mouse_move(rel_x, rel_y);
			if ((event.ie_key.flags & IE_KEY_RELEASE) && event.ie_key.scancode == INPUT_KEY_MOUSE_LEFT && grab) {
				grab = NULL;
				// sill forward the event so the window know the dragging stoped
			}
			if (!grab_input && (event.ie_key.flags & IE_KEY_PRESS)) {
				window_t *window = get_window_at(cursor.x, cursor.y);
				if (!window) continue;
				update_focus(window);
			}

			if (!focus_window) continue;
			// forward event to the focus window
			twm_event_input_t twm_event = {
				.base = {
					.size = sizeof(twm_event),
					.type = TWM_EVENT_INPUT,
				},
				.window = focus_window->id,
				.type = TWM_INPUT_KEY,
				.key = {
					.key = event.ie_key.scancode,
					.scancode = event.ie_key.scancode,
				}
			};
			if (event.ie_key.flags & IE_KEY_PRESS) {
				twm_event.key.flags |= TWM_INPUT_PRESS;
			} else if (event.ie_key.flags & IE_KEY_RELEASE) {
				twm_event.key.flags |= TWM_INPUT_RELEASE;
			} else if (event.ie_key.flags & IE_KEY_HOLD) {
				twm_event.key.flags |= TWM_INPUT_HOLD;
			}
			send_event(get_client(focus_window->client), (twm_event_t *)&twm_event);
		}
	}
	flush_mouse_move(rel_x, rel_y);
}

void handle_keyboard(void) {
	struct input_event input_event;
	while (libinput_get_keyboard_event(kb, &input_event) >= 0) {
		if (!focus_window) return;

		// forward only key events
		if (input_event.ie_type != IE_KEY_EVENT) return;

		// forward event to the focus window
		twm_event_input_t event = {
			.base = {
				.size = sizeof(event),
				.type = TWM_EVENT_INPUT,
			},
			.window = focus_window->id,
			.type = TWM_INPUT_KEY,
			.key = {
				.key = input_event.ie_key.key,
				.scancode = input_event.ie_key.scancode,
			}
		};

		if (input_event.ie_key.flags & IE_KEY_PRESS) {
			event.key.flags |= TWM_INPUT_PRESS;
		} else if (input_event.ie_key.flags & IE_KEY_RELEASE) {
			event.key.flags |= TWM_INPUT_RELEASE;
		} else if (input_event.ie_key.flags & IE_KEY_HOLD) {
			event.key.flags |= TWM_INPUT_HOLD;
		}

		send_event(get_client(focus_window->client), (twm_event_t *)&event);
	}
}