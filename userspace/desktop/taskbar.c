#include <tgui/tgui.h>
#include <desktop.h>
#include <twm.h>

static tgui_vector_t *taskbutton_list;

static twm_window_t window_from_button(tgui_list_item_t *list_item) {
	return (twm_window_t)(uintptr_t)list_item->item;
}

static void taskbutton_click(tgui_list_item_t *list_item) {
	twm_window_t window = window_from_button(list_item);
	twm_window_attr_t attr;
	twm_get_window_attr(window, &attr);

	if (attr.attr & TWM_ATTR_MINIMIZED) {
		twm_window_restore(window);
	} else {
		twm_window_minimize(window);
	}
}

static int taskbutton_factory_setup(tgui_factory_t *factory, tgui_list_item_t *list_item) {
	(void)factory;
	tgui_button_t *button = tgui_button_new();
	tgui_list_item_set_child(list_item, TGUI_WIDGET_CAST(button));
	tgui_widget_connect_signal(TGUI_WIDGET_CAST(list_item), "click", TCALLBACK_CAST(taskbutton_click), NULL);
	return 0;
}

static int taskbutton_factory_bind(tgui_factory_t *factory, tgui_list_item_t *list_item) {
	(void)factory;
	tgui_button_t *button = TGUI_BUTTON_CAST(tgui_list_item_get_child(list_item));
	twm_window_t window = window_from_button(list_item);
	twm_window_attr_t attr;
	twm_get_window_attr(window, &attr);
	tgui_button_set_text(button, attr.title);
	return 0;
}

static tgui_factory_t taskbutton_factory = {
	.setup = taskbutton_factory_setup,
	.bind = taskbutton_factory_bind,
};

void init_taskbar(void) {
	taskbutton_list = tgui_vector_new();
}

void create_taskbar(twm_screen_t screen) {
	twm_screen_attr_t screen_attr;
	twm_get_screen_attr(screen, &screen_attr);

	tgui_window_t *taskbar = tgui_window_new("taskbar", screen_attr.fb_info.width, 50);
	tgui_surface_set_position(TGUI_SURFACE_CAST(taskbar), 0, screen_attr.fb_info.height - 50);
	tgui_window_set_title_bar(taskbar, TGUI_FALSE);
	twm_window_t twm_window = tgui_surface_get_twm_window(TGUI_SURFACE_CAST(taskbar));
	twm_window_set_zindex(twm_window, TWM_ZINDEX_MAX);
	tgui_box_t *main_box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(main_box), TGUI_TRUE);
	tgui_widget_set_orientation(TGUI_WIDGET_CAST(main_box), TGUI_ORIENTATION_HORIZONTAL);
	tgui_window_set_child(taskbar, TGUI_WIDGET_CAST(main_box));

	tgui_popover_t *start_menu = tgui_popover_new();
	tgui_list_view_t *start_menu_list = tgui_list_view_new(&app_factory, TGUI_LIST_MODEL_CAST(app_list));
	tgui_popover_set_child(start_menu, TGUI_WIDGET_CAST(start_menu_list));

	tgui_popover_button_t *start_button = tgui_popover_button_new(start_menu, "");
	tgui_popover_button_set_direction(start_button, TGUI_DIRECTION_TOP);
	tgui_button_set_icon(TGUI_BUTTON_CAST(start_button), "stanix24");
	tgui_box_append_widget(main_box, TGUI_WIDGET_CAST(start_button));

	tgui_list_view_t *task_buttons = tgui_list_view_new(&taskbutton_factory, TGUI_LIST_MODEL_CAST(taskbutton_list));
	tgui_widget_set_orientation(TGUI_WIDGET_CAST(task_buttons), TGUI_ORIENTATION_HORIZONTAL);
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(task_buttons), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(task_buttons), TGUI_TRUE);
	tgui_box_append_widget(main_box, TGUI_WIDGET_CAST(task_buttons));
}

static size_t find_index(twm_window_t window) {
	for (size_t i=0; i < tgui_list_model_get_count(TGUI_LIST_MODEL_CAST(taskbutton_list)); i++) {
		void *data = tgui_list_model_get_item(TGUI_LIST_MODEL_CAST(taskbutton_list), i);
		twm_window_t current = (twm_window_t)(uintptr_t)data;
		if (current == window) {
			return i;
		}
	}
	return SIZE_MAX;
}

void taskbar_add_window(twm_window_t window) {
	tgui_vector_append(taskbutton_list, (void *)(uintptr_t)window);
}

void taskbar_remove_window(twm_window_t window) {
	size_t index = find_index(window);
	if (index == SIZE_MAX) return;
	tgui_vector_remove(taskbutton_list, index);
}

void taskbar_update_window(twm_window_t window) {
	size_t index = find_index(window);
	if (index == SIZE_MAX) return;
	tgui_list_model_update(TGUI_LIST_MODEL_CAST(taskbutton_list), index, 1, 1);
}
