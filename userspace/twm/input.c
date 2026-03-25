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
        if (grab) {
            move_window(grab, new_x + grab_offset_x, new_y + grab_offset_y);
        }
        move_cursor(&cursor, new_x, new_y);
        return;
    }
    if (event.ie_type == IE_KEY_EVENT) {
        if (event.ie_key.flags & IE_KEY_RELEASE && event.ie_key.scancode == INPUT_KEY_MOUSE_LEFT) {
            grab = NULL;
        } else if (event.ie_key.flags & IE_KEY_PRESS) { 
            window_t *window = get_window_at(cursor.x, cursor.y);
            if (!window) return;

            // allow to grab/ungrab windows
            if (event.ie_key.scancode == INPUT_KEY_MOUSE_LEFT && is_inside_titlebar(window, cursor.x, cursor.y, 0, 0)) {
                update_focus(window);
                if (!grab) {
                    // keep grab offset
                    grab = window;
                    grab_offset_x = window->x - cursor.x;
                    grab_offset_y = window->y - cursor.y;
                }
            }
        }
    }
}

void handle_keyboard(void) {
    struct input_event event;
    if (libinput_get_event(kb, &event) < 0) return;
}