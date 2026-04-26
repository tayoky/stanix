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

#endif
