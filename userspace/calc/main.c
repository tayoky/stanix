#include <stdio.h>
#include <stdlib.h>
#include <tgui/tgui.h>

tgui_text_t *label;
int syntax_error;

long parse_value(const char **ptr) {
	char *end;
	long value = strtol(*ptr, &end, 10);
	if (end == *ptr) {
		syntax_error = 1;
		return 0;
	}
	*ptr = end;
	return value;
}

long parse_mul(const char **_ptr) {
	const char *ptr = *_ptr;
	long value = parse_value(&ptr);
	if (syntax_error) return 0;
	while (*ptr) {
		switch (*ptr) {
		case '*':
		case '/':;
			int op = *ptr;
			ptr++;
			long new_val = parse_value(&ptr);
			if (syntax_error) return 0;
			if (op == '*') {
				value *= new_val;
			} else {
				value /= new_val;
			}
			break;
		default:
			goto finish;
		}
	}
finish:
	*_ptr = ptr;
	return value;
}

long calculate1(const char **_ptr) {
	const char *ptr = *_ptr;
	long value = parse_mul(&ptr);
	if (syntax_error) return 0;

	while (*ptr) {
		switch (*ptr) {
		case '+':
		case '-':;
			int op = *ptr;
			ptr++;
			long new_val = parse_mul(&ptr);
			if (syntax_error) return 0;
			if (op == '+') {
				value += new_val;
			} else {
				value -= new_val;
			}
			break;
		default:
			syntax_error = 1;
			return 0;
		}
	}
	*_ptr = ptr;
	return value;
}

int button_click(tgui_event_t *event) {
	puts("click");
	tgui_button_t *button = TGUI_BUTTON_CAST(event->widget);
	tgui_text_insert(label, tgui_button_get_text(button));
	return TGUI_EVENT_HANDLED;
}

int equal_click(tgui_event_t *event) {
	const char *str = tgui_text_get_content(label);
	syntax_error = 0;
	long value = calculate1(&str);
	if (*str) syntax_error = 1;
	tgui_text_set_content(label, "");
	if (syntax_error) {
		tgui_text_set_placeholder(label, "syntax error");
	} else {
		char text[64];
		sprintf(text, "%ld", value);
		tgui_text_set_placeholder(label, text);
	}
	return TGUI_EVENT_HANDLED;
}
	
tgui_button_t *new_button(const char *text) {
	tgui_button_t *button = tgui_button_new();
	tgui_widget_set_margin(TGUI_WIDGET_CAST(button), 1);
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(button), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(button), TGUI_TRUE);
	tgui_button_set_text(button, text);
	tgui_widget_set_callback(TGUI_WIDGET_CAST(button), TGUI_EVENT_CLICK, button_click, NULL);
	return button;
}

int main() {
	if (tgui_init() < 0) {
		puts("fail to init tgui");
		return -1;
	}

	tgui_window_t *window = tgui_window_new("tgui calculator", 200, 200);
	tgui_window_set_scaling(window, 2);

	label = tgui_text_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(label), TGUI_TRUE);

	// create the grid with all the buttons
	tgui_grid_t *grid = tgui_grid_new(4, 4);
	tgui_grid_set_same_width(grid, TGUI_TRUE);
	tgui_grid_set_same_height(grid, TGUI_TRUE);
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(grid), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(grid), TGUI_TRUE);
	for (int i=1; i<10; i++) {
		int x = (i - 1) % 3;
		int y = 2 - ((i - 1)/ 3);

		char text[10];
		sprintf(text, "%d", i);
		tgui_button_t *button = new_button(text);
		tgui_grid_set_at(grid, x, y, TGUI_WIDGET_CAST(button));
	}
	tgui_button_t *equal = new_button("=");
	tgui_widget_set_callback(TGUI_WIDGET_CAST(equal), TGUI_EVENT_CLICK, equal_click, NULL);
	tgui_grid_set_at(grid, 0, 3, TGUI_WIDGET_CAST(equal));
	tgui_grid_set_at(grid, 1, 3, TGUI_WIDGET_CAST(new_button("0")));
	tgui_grid_set_at(grid, 3, 0, TGUI_WIDGET_CAST(new_button("/")));
	tgui_grid_set_at(grid, 3, 1, TGUI_WIDGET_CAST(new_button("*")));
	tgui_grid_set_at(grid, 3, 2, TGUI_WIDGET_CAST(new_button("-")));
	tgui_grid_set_at(grid, 3, 3, TGUI_WIDGET_CAST(new_button("+")));

	// put the label and the grid in a box
	tgui_box_t *box = tgui_box_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(box), TGUI_TRUE);
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(label));
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(tgui_separator_new(TGUI_ORIENTATION_HORIZONTAL)));
	tgui_box_append_widget(box, TGUI_WIDGET_CAST(grid));

	tgui_window_set_child(window, TGUI_WIDGET_CAST(box));

	tgui_main();
	tgui_fini();
}
