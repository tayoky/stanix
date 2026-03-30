#include <twm-internal.h>
#include <libinput.h>

static window_t *grab = NULL;
static long grab_offset_x;
static long grab_offset_y;

// handle input events
void handle_mouse(void) {
    struct input_event event;
    if (libinput_get_event(mouse, &event) < 0) return;
    if (event.ie_type == IE_MOVE_EVENT) {
        long new_x = cursor.x + event.ie_move.x;
        long new_y = cursor.y + event.ie_move.y;
        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        if (new_x + theme.cursor_texture->width  > (size_t)gfx->width)  new_x = gfx->width  - theme.cursor_texture->width;
        if (new_y + theme.cursor_texture->height > (size_t)gfx->height) new_y = gfx->height - theme.cursor_texture->height;
        move_cursor(&cursor, new_x, new_y);
        if (grab) {
            move_window(grab, new_x + grab_offset_x, new_y + grab_offset_y);
        } else {
            // forward event to the window
            window_t *window = get_window_at(cursor.x, cursor.y);
            if (!window) return;
            twm_event_input_t twm_event = {
                .base = {
                    .size = sizeof(twm_event),
                    .type = TWM_EVENT_INPUT,
                },
                .window = window->id,
                .type = TWM_INPUT_MOVE,
                .move = {
                    .abs_x = cursor.x,
                    .abs_y = cursor.y,
                    .rex_x = event.ie_move.x,
                    .rel_y = event.ie_move.y,
                },
            };

            send_event(get_client(window->client), (twm_event_t*)&twm_event);
        }
        return;
    }
    if (event.ie_type == IE_KEY_EVENT) {
        if (event.ie_key.flags & IE_KEY_RELEASE && event.ie_key.scancode == INPUT_KEY_MOUSE_LEFT) {
            grab = NULL;
        } else if (event.ie_key.flags & IE_KEY_PRESS) {
            window_t *window = get_window_at(cursor.x, cursor.y);
            if (!window) return;
            update_focus(window);

            // allow to grab/ungrab windows
            if (event.ie_key.scancode == INPUT_KEY_MOUSE_LEFT && is_inside_titlebar(window, cursor.x, cursor.y, 0, 0)) {
                if (!grab) {
                    // keep grab offset
                    grab = window;
                    grab_offset_x = window->x - cursor.x;
                    grab_offset_y = window->y - cursor.y;
                }
                return;
            }
        }
		if (!focus_window) return;
        // forward event to the focus window
        twm_event_input_t twm_event = {
            .base = {
                .size = sizeof(twm_event),
                .type = TWM_EVENT_INPUT,
            },
            .window = focus_window->id,
            .type = TWM_INPUT_KEY,
			.key = {
				.key = event.ie_key.key,
				.scancode = event.ie_key.key,
			}
		};
		if (event.ie_key.flags & IE_KEY_PRESS) {
			twm_event.key.flags |= TWM_INPUT_PRESS;
		} else if (event.ie_key.flags & IE_KEY_RELEASE) {
			twm_event.key.flags |= TWM_INPUT_RELEASE;
		} else if (event.ie_key.flags & IE_KEY_HOLD) {
			twm_event.key.flags |= TWM_INPUT_HOLD;
		}
        send_event(get_client(focus_window->client), (twm_event_t*)&twm_event);
    }
}

void handle_keyboard(void) {
    struct input_event input_event;
    if (libinput_get_keyboard_event(kb, &input_event) < 0) return;
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

    send_event(get_client(focus_window->client), (twm_event_t*)&event);
}