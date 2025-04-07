#ifndef MODULE_H
#define MODULE_H

void __export_symbol(void *sym,const char *name);
void __unexport_symbol(void *sym,const char *name);

#define EXPORT(sym) __export_symbol(sym,#sym);
#define UNEXPORT(sym) __unexport_symbol(sym,#sym);

#endif