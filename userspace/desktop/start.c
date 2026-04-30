#include <tgui/tgui.h>
#include <unistd.h>
#include <stdlib.h>
#include <desktop.h>

static void app_click(tgui_list_item_t *list_item) {
	app_t *app = list_item->item;
	if (app->command) {
		pid_t pid = fork();
		if (!pid) {
			execl(getenv("SHELL"), getenv("SHELL"), "-c", app->command);
			exit(1);
		}
		// TODO : wait or something
	}
}

static int app_factory_setup(tgui_factory_t *factory, tgui_list_item_t *list_item) {
	(void)factory;
	tgui_button_t *button = tgui_button_new();
	tgui_list_item_set_child(list_item, TGUI_WIDGET_CAST(button));
	tgui_widget_connect_signal(TGUI_WIDGET_CAST(list_item), "click", TCALLBACK_CAST(app_click), NULL);
	return 0;
}

static int app_factory_bind(tgui_factory_t *factory, tgui_list_item_t *list_item) {
	(void)factory;
	tgui_button_t *button = TGUI_BUTTON_CAST(tgui_list_item_get_child(list_item));
	app_t *app = list_item->item;
	if (app->icon) {
		tgui_button_set_icon(button, app->icon);
	} else {
		tgui_button_set_text(button, app->name);
	}
	return 0;
}

tgui_factory_t app_factory = {
	.setup = app_factory_setup,
	.bind = app_factory_bind,
};
