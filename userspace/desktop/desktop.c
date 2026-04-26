#include <stdio.h>
#include <stdlib.h>
#include <tgui/tgui.h>
#include <libutils/hashmap.h>
#include <twm.h>
#include <dirent.h>
#include <libini.h>
#include <desktop.h>

tgui_window_t *window;
tgui_box_t *main_box;
tgui_popover_t *start_menu;
tgui_popover_button_t *start_button;
tgui_vector_t *app_list;
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
	app_list = tgui_vector_new();
	tgui_list_view_t *start_menu_list = tgui_list_view_new(&app_factory, app_list);
	tgui_popover_set_child(start_menu, TGUI_WIDGET_CAST(start_menu_list));

	// TODO : cleanup
	DIR *dir = opendir("/etc/desktop.d");
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir))) {
			char full_path[sizeof(entry->d_name) + 16];
			// ignore hidden entries
			if (entry->d_name[0] == '.') continue;
			snprintf(full_path, sizeof(full_path), "/etc/desktop.d/%s", entry->d_name);
			utils_shashmap_t *data = ini_parse_file(full_path);
			if (!data) continue;

			app_t *app = malloc(sizeof(app_t));
			app->name    = utils_shashmap_get(data, "name");
			app->icon    = utils_shashmap_get(data, "icon");
			app->command = utils_shashmap_get(data, "command");
			tgui_vector_append(app_list, app);
		}
		closedir(dir);
	}

	start_button = tgui_popover_button_new(start_menu, "");
	tgui_popover_button_set_direction(start_button, TGUI_DIRECTION_TOP);
	tgui_button_set_icon(TGUI_BUTTON_CAST(start_button), "stanix24");
	tgui_box_append_widget(main_box, TGUI_WIDGET_CAST(start_button));

	// setup desktop hook
	twm_set_handler(TWM_EVENT_DESKTOP, desktop_hook, NULL);
	twm_grab_desktop_hook();

	tgui_main();
	tgui_fini();
}
