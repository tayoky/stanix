#include <stdio.h>
#include <tgui/tgui.h>

/**
 * this file comes from libtgui (https://github.com/tayoky/tgui) and is under the BSD clause 3 license
 */

tgui_window_t *window;

int close_window(tgui_event_t *event) {
	if (event->widget == TGUI_WIDGET_CAST(window)) {
		tgui_quit();
		return TGUI_EVENT_HANDLED;
	} else {
		return TGUI_EVENT_NOT_HANDLED;
	}
}

void add_element(tgui_box_t *box, tgui_widget_t *widget) {
	char text[256];
	sprintf(text, "this is is a %s", tgui_widget_type_from_object(widget)->name);
	tgui_label_t *label = tgui_label_new(text);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(label));
	tgui_widget_set_hexpand(widget, TGUI_TRUE);
	tgui_box_append_widget(box, widget);
}

void add_text(tgui_box_t *box, const char *text) {
	tgui_label_t *label = tgui_label_new(text);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(label));
}

void add_button(tgui_box_t *box, int align) {
	tgui_button_t *button = tgui_button_new();
	tgui_button_set_text(button, "button");
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(button), TGUI_TRUE);
	tgui_widget_set_halign(TGUI_WIDGET_CAST(button), align);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(button));
}

tgui_widget_t *input_tab() {
	tgui_box_t *box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);

	tgui_button_t *button = tgui_button_new();
	tgui_button_set_text(button, "button");
	add_element(box, TGUI_WIDGET_CAST(button));

	tgui_slider_t *slider = tgui_slider_new(TGUI_ORIENTATION_HORIZONTAL);
	tgui_slider_set_value(slider, 0.3f);
	add_element(box, TGUI_WIDGET_CAST(slider));

	tgui_scrollbar_t *scrollbar = tgui_scrollbar_new(TGUI_ORIENTATION_HORIZONTAL);
	tgui_scrollbar_set_total_size(scrollbar, 600);
	tgui_scrollbar_set_view_size(scrollbar, 100);
	add_element(box, TGUI_WIDGET_CAST(scrollbar));
	return TGUI_WIDGET_CAST(box);
}

tgui_widget_t *align_tab() {
	tgui_box_t *box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);

	add_text(box, "left");
	add_button(box, TGUI_ALIGN_LEFT);
	add_text(box, "center");
	add_button(box, TGUI_ALIGN_CENTER);
	add_text(box, "right");
	add_button(box, TGUI_ALIGN_RIGHT);
	add_text(box, "fill");
	add_button(box, TGUI_ALIGN_FILL);

	return TGUI_WIDGET_CAST(box);
}

tgui_widget_t *list_tab() {
	tgui_box_t *box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	static tgui_list_t list;
	tgui_list_append(&list, &tgui_string_item_new("item1")->node);
	tgui_list_append(&list, &tgui_string_item_new("item2")->node);
	tgui_list_append(&list, &tgui_string_item_new("item3")->node);

	tgui_list_view_t *list_view = tgui_list_view_new(tgui_string_factory(), &list);
	add_element(box, TGUI_WIDGET_CAST(list_view));

	return TGUI_WIDGET_CAST(box);
}

tgui_widget_t *menu_tab() {
	tgui_box_t *box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_widget_set_orientation(TGUI_WIDGET_CAST(box), TGUI_ORIENTATION_HORIZONTAL);

	tgui_popover_t *popover = tgui_popover_new();
	tgui_popover_set_child(popover, TGUI_WIDGET_CAST(tgui_label_new("this is a popover")));
	add_element(box, TGUI_WIDGET_CAST(tgui_submenu_button_new(popover, "popover")));


	return TGUI_WIDGET_CAST(box);
}

int main() {
	if (tgui_init() < 0) {
		puts("fail to init twm");
		return 1;
	}

	window = tgui_window_new("tgui input test", 640, 480);
	tgui_widget_set_callback(TGUI_WIDGET_CAST(window), TGUI_EVENT_DESTROY, close_window, NULL);
	
	tgui_box_t *box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_window_set_child(window, TGUI_WIDGET_CAST(box));

	tgui_stack_switcher_t *tabs = tgui_stack_switcher_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(tabs), TGUI_TRUE);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(tabs));

	tgui_stack_t *stack = tgui_stack_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(stack), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(stack), TGUI_TRUE);
	tgui_stack_switcher_set_stack(tabs, stack);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(stack));

	tgui_stack_add_child(stack, input_tab(), "inputs");
	tgui_stack_add_child(stack, align_tab(), "aligns");
	tgui_stack_add_child(stack, list_tab(), "lists");
	tgui_stack_add_child(stack, menu_tab(), "menus");
	tgui_stack_switcher_update(tabs);
	tgui_main();
	tgui_fini();
	return 0;
}
