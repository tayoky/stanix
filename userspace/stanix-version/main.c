#include <stdio.h>
#include <libini.h>
#include <tgui/tgui.h>

void add_label(tgui_box_t *box, const char *text) {
	tgui_label_t *label = tgui_label_new(text);
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(label), TGUI_TRUE);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(label));
}

int main() {
	if (tgui_init() < 0) {
		puts("cannot init tgui");
		return 1;
	}
	utils_shashmap_t *infos = ini_parse_file("/etc/os-release");
	if (!infos) {
		puts("could not open /etc/os-release");
		return 1;
	}
	tgui_window_t *window = tgui_window_new("stanix version", 640, 480);
	if (!window) {
		return 1;
	}
	tgui_box_t *box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_window_set_child(window, TGUI_WIDGET_CAST(box));

	tgui_icon_t *icon = tgui_icon_new("stanix128");
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(icon));

	tgui_separator_t *separator = tgui_separator_new(TGUI_ORIENTATION_HORIZONTAL);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(separator));

	add_label(box, "The Stanix operating system");
	char version[256];
	sprintf(version, "Version %s", utils_shashmap_get(infos, "VERSION"));
	add_label(box, version);
	add_label(box, "Copyright Tayoky 2024-2026 (GPL3)");
	add_label(box, "This program is free software: you can redistribute it and/or modify");
	add_label(box, "it under the terms of the GNU General Public License as published by");
	add_label(box, "the Free Software Foundation, either version 3 of the License, or");
	add_label(box, "(at your option) any later version.");

	tgui_main();

	tgui_widget_destroy(TGUI_WIDGET_CAST(window));
	return 0;
}
