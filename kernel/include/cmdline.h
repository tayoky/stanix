#ifndef KERNEL_CMDLINE_H
#define KERNEL_CMDLINE_H

void kcmdline_set(const char *cmdline);
const char *kcmdline_get(void);
int kcmdline_have_opt(const char *opt);
const char *kcmdline_get_option(constcchar *opt);

#endif
