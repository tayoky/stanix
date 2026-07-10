#include <stdio.h>
#include <stdlib.h>
#include <tgui/tgui.h>
#include <libutils/hashmap.h>
#include <twm.h>
#include <dirent.h>
#include <libini.h>
#include <desktop.h>

tgui_vector_t *app_list;

int desktop_hook(twm_event_t *event, void *arg) {
	(void)arg;
	if (event->type != TWM_EVENT_DESKTOP) return TGUI_FALSE;
	puts("recive event");
	twm_event_desktop_t *desktop_event = (twm_event_desktop_t *)event;
	switch (desktop_event->type) {
	case TWM_WINDOW_CREATED:
		taskbar_add_window(desktop_event->id);
		break;
	case TWM_WINDOW_DESTROYED:
		taskbar_remove_window(desktop_event->id);
		break;
	case TWM_WINDOW_UPDATED:
		taskbar_update_window(desktop_event->id);
		break;
	}
	return TGUI_TRUE;
}

int main() {
	if (tgui_init() < 0) {
		puts("fail to init libtgui");
		return 1;
	}

	app_list = tgui_vector_new();

	init_taskbar();
	// TODO : multi screen support
	create_taskbar(1);

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

	// setup desktop hook
	tgui_register_platform_handler((int(*)(void*,void*))desktop_hook, NULL);
	twm_grab_desktop_hook();

	tgui_main();
	tgui_fini();
}
