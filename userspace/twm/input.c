#include <twm-internal.h>
#include <libinput.h>

// handle input events
void handle_mouse(void) {
    struct input_event event;
    if (libinput_get_event(mouse, &event) < 0) return;
    if (event.ie_type == IE_MOVE_EVENT) {
        long new_x = cursor.x + event.ie_move.x;
        long new_y = cursor.y + event.ie_move.y;
        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        render_and_move_cursor(&cursor, new_x, new_y);
    }
}

void handle_keyboard(void) {
    struct input_event event;
    if (libinput_get_event(kb, &event) < 0) return;
}