#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>

typedef struct kmodule_struct {
	uint64_t magic;
	char *name;
	char *description;
	char *license;
	char *author;
	int (*init)(int,char **);
	int (*fini)(void);
	void *base;               //the base address of the module
} kmodule;

void __export_symbol(void *sym,const char *name);
void __unexport_symbol(void *sym,const char *name);

int insmod(const char *pathname,const char **args,char **name);
int rmmod(const char *name);

void init_mod();

/// @brief check if an module was launch with a specified argument
/// @param argc the number of argument
/// @param argv an pointer to the argument list
/// @param option the option to check
/// @return 1 if the argument is present or 0 if not
int have_opt(int argc,char **argv,char *option);

#define EXPORT(sym) __export_symbol(&sym,#sym);
#define UNEXPORT(sym) __unexport_symbol(&sym,#sym);

#define MODULE_MAGIC 0x13082011

#ifdef MODULE
extern kmodule module_meta;
#endif

#endif