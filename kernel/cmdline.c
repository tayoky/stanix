#include <kernel/cmdline.h>
#include <kernel/string.h>

static const char *kcmdline;

void kcmdline_set(const char *cmdline) {
    kcmdline = cmdline;
}

const char *kcmdline_get(void) {
    return kcmdline;
}

int kcmdline_have_opt(const char *opt) {
    return strstr(kcmdline, opt) != NULL;
}
