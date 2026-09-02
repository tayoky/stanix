#include <kernel/cmdline.h>
#include <kernel/string.h>
#include <kernel/macro.h>
#include <kernel/print.h>

static const char *kcmdline;
static char dup[1024];
static const char *args[64];
static size_t args_count = 0;

void kcmdline_set(const char *cmdline) {
    kcmdline = cmdline;
	snprintf(dup, sizeof(dup), "%s", kcmdline);
	args_count = 0;

	char *ptr;
	char *arg = strtok_r(dup, " \t", &ptr);
	while (arg && args_count < arraylen(args)) {
		args[args_count++] = arg;
		arg = strtok_r(NULL, " \t", &ptr);
	}
}

const char *kcmdline_get(void) {
    return kcmdline;
}

static const char *get_opt(const char *opt) {
	for (size_t i = 0; i < args_count; i++) {
		if (!strcmp(args[i], opt)) {
			return args[i];
		}
		const char *equal = strchr(args[i], '=');
		if (equal) {
			size_t len = equal - args[i];
			if (len == strlen(opt) && !memcmp(args[i], opt, len)) {
				return args[i];
			}
		}
	}
	return NULL;
}

int kcmdline_have_opt(const char *opt) {
	return get_opt(opt) != NULL;
}

const char *kcmdline_get_option(const char *opt) {
	const char *raw = get_opt(opt);
	if (!raw) return NULL;
	const char *value = strchr(raw, '=');
	if (!value) return NULL;
	return value + 1;
}
