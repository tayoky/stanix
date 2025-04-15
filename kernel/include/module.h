#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>

typedef struct kmodule_struct {
	uint64_t magic;
	char *name;
	char *description;
	char *liscence;
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

#define EXPORT(sym) __export_symbol(sym,#sym);
#define UNEXPORT(sym) __unexport_symbol(sym,#sym);

#define MODULE_MAGIC 0x13082011

#ifdef MODULE
extern kmodule module_meta;
#endif

#endif