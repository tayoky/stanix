#ifndef _LIBINPUT_H
#define _LIBINPUT_H

#include <input.h>

int libinput_open(const char *path, int flags);
int libinput_close(int fd);
int libinput_get_event(int fd, struct input_event *event);
int libinput_get_info(int fd, struct input_info *info);
const char *libinput_class_string(unsigned long class);
const char *libinput_subclass_string(unsigned long class, unsigned long subclass);

#endif