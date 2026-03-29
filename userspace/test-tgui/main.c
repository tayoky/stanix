#include <stdio.h>
#include <tgui/tgui.h>

int main() {
	if (tgui_init() < 0) {
		puts("cannot init tgui");
		return 1;
	}
	tgui_window_t *window = tgui_window_new("tgui window", 640, 480);
	if (!window) {
		return 1;
	}
	tgui_color_t *green = tgui_color_new_rgb(0, 200, 0);
	tgui_box_t *box = tgui_box_new();
	tgui_window_set_child(window, TGUI_WIDGET_CAST(box));
	tgui_widget_set_padding(TGUI_WIDGET_CAST(box), 5);
	
	
	// put three buttons in the box
	for (int i=0; i<3; i++) {
		tgui_button_t *button = tgui_button_new();
		char text[64];
		sprintf(text, "hi i'm button %d", i);
		tgui_button_set_text(button, text);
		tgui_box_append_widget(box, TGUI_WIDGET_CAST(button));
	}
	
	tgui_main();
	
	tgui_widget_destroy(TGUI_WIDGET_CAST(window));
	return 0;
}
