#include <kernel/module.h>
#include <kernel/print.h>


//simple module to test the dynamic module loading

void test_export(){
	kdebugf("export work !!!\n");
}

int init(int argc,char **argv){
	kdebugf("hello world from test.ko !!!\n");
	EXPORT(test_export);
	return 0;
}

int fini(){
	UNEXPORT(test_export);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init,
	.fini = fini,
	.author = "tayoky",
	.name = "test",
	.description = "just a simple module to test",
	.liscence = "GPL 3",
};