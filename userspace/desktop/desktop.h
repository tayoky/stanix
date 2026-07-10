#ifndef DESKTOP_H
#define DESKTOP_H

#include <tgui/tgui.h>
#include <sys/types.h>

typedef struct app {
    char *icon;
    char *name;
    char *command;
} app_t;

extern tgui_factory_t app_factory;
extern tgui_vector_t *app_list;

void init_taskbar(void);
void create_taskbar(twm_screen_t screen);
void taskbar_add_window(twm_window_t window);
void taskbar_remove_window(twm_window_t window);
void taskbar_update_window(twm_window_t window);
#endif
