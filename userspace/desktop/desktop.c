#include <stdio.h>
#include <stdlib.h>
#include <tgui/tgui.h>
#include <libutils/hashmap.h>
#include <twm.h>

tgui_window_t *window;
tgui_box_t *main_box;
tgui_button_t *start_button;
utils_hashmap_t buttons;

void desktop_hook(twm_event_t *event, void *arg) {
	(void)arg;
	puts("recive event");
	twm_event_desktop_t *desktop_event = (twm_event_desktop_t *)event;
	switch (desktop_event->type) {
	case TWM_WINDOW_CREATED:;
		tgui_button_t *button = tgui_button_new();
		tgui_button_set_text(button, "window");
		tgui_box_append_widget(main_box, TGUI_WIDGET_CAST(button));
		utils_hashmap_add(&buttons, desktop_event->id, button);
		break;
	case TWM_WINDOW_DESTROYED:;
		button = utils_hashmap_get(&buttons, desktop_event->id);
		if (!button) break;
		tgui_widget_destroy(TGUI_WIDGET_CAST(button));
		utils_hashmap_remove(&buttons, desktop_event->id);
		break;
	}
}

int main() {
	if (tgui_init() < 0) {
		puts("fail to init libtgui");
		return 1;
	}

	utils_init_hashmap(&buttons, 128);

	window = tgui_window_new("taskbar", 600, 100);
	tgui_window_set_title_bar(window, TGUI_FALSE);
	main_box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(main_box), TGUI_TRUE);
	tgui_widget_set_orientation(TGUI_WIDGET_CAST(main_box), TGUI_ORIENTATION_HORIZONTAL);
	tgui_window_set_child(window, TGUI_WIDGET_CAST(main_box));
	start_button = tgui_button_new();
	tgui_button_set_icon(start_button, "stanix24");
	tgui_box_append_widget(main_box, TGUI_WIDGET_CAST(start_button));

	// setup desktop hook
	twm_set_handler(TWM_EVENT_DESKTOP, desktop_hook, NULL);
	twm_grab_desktop_hook();

	tgui_main();
	tgui_fini();
}
