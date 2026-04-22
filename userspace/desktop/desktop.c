#include <stdio.h>
#include <stdlib.h>
#include <tgui/tgui.h>
#include <libutils/hashmap.h>
#include <twm.h>
#include <dirent.h>
#include <libini.h>

tgui_window_t *window;
tgui_box_t *main_box;
tgui_popover_t *start_menu;
tgui_button_t *start_button;
utils_hashmap_t buttons;

void desktop_hook(twm_event_t *event, void *arg) {
	(void)arg;
	puts("recive event");
	twm_event_desktop_t *desktop_event = (twm_event_desktop_t *)event;
	switch (desktop_event->type) {
	case TWM_WINDOW_CREATED:;
		twm_window_attr_t attr;
		twm_get_window_attr(desktop_event->id, &attr);
		
		// only show top domain window
		if (attr.parent != TWM_NULL) break;

		tgui_button_t *button = tgui_button_new();
		tgui_button_set_text(button, attr.title);
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

void start_click (void) {
	tgui_popover_set_position(start_menu, -100, 0);
	tgui_popover_popup(start_menu);
}

int main() {
	if (tgui_init() < 0) {
		puts("fail to init libtgui");
		return 1;
	}

	utils_init_hashmap(&buttons, 128);

	twm_fb_info_t screen;
	twm_get_screen_fb(0, &screen);

	window = tgui_window_new("taskbar", screen.width, 50);
	tgui_surface_set_position(TGUI_SURFACE_CAST(window), 0, screen.height - 50);
	tgui_window_set_title_bar(window, TGUI_FALSE);
	main_box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(main_box), TGUI_TRUE);
	tgui_widget_set_orientation(TGUI_WIDGET_CAST(main_box), TGUI_ORIENTATION_HORIZONTAL);
	tgui_window_set_child(window, TGUI_WIDGET_CAST(main_box));

	start_menu = tgui_popover_new();
	tgui_box_t *start_menu_list = tgui_box_new();
	tgui_popover_set_child(start_menu, TGUI_WIDGET_CAST(start_menu_list));

	// TODO : cleanup
	DIR *dir = opendir("/etc/desktop.d");
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir))) {
			char full_path[sizeof(entry->d_name) + 16];
			snprintf(full_path, sizeof(full_path), "/etc/desktop.d/%s", entry->d_name);
			utils_shashmap_t *data = ini_parse_file(full_path);
			if (!data) continue;

			tgui_button_t *button = tgui_button_new();
			tgui_button_set_text(button, utils_shashmap_get(data, "name"));
			tgui_box_append_widget(start_menu_list, TGUI_WIDGET_CAST(button));
		}
		closedir(dir);
	}

	start_button = tgui_button_new();
	tgui_button_set_icon(start_button, "stanix24");
	tgui_widget_connect_signal(TGUI_WIDGET_CAST(start_button), "click", TCALLBACK_CAST(start_click), NULL);
	tgui_box_append_widget(main_box, TGUI_WIDGET_CAST(start_button));

	// setup desktop hook
	twm_set_handler(TWM_EVENT_DESKTOP, desktop_hook, NULL);
	twm_grab_desktop_hook();

	tgui_main();
	tgui_fini();
}
