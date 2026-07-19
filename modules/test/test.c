#include <kernel/module.h>
#include <kernel/print.h>


//simple module to test the dynamic module loading

void test_export(){
	kdebugf("export work !!!\n");
}

int init(int argc,char **argv){
	kdebugf("hello world from kernel module !!!\n");
	kdebugf("module loaded at address 0x%p\n",module_meta.base);
	kdebugf("args :\n");
	for (int i = 0; i < argc; i++){
		kdebugf("arg%d : %s\n",i,argv[i]);
	}

	//test.ko can crash kernel if needed
	if(have_opt(argc,argv,"--crash")){
		*(uint64_t *)0xFFF = 0x1234;
	}
	
	EXPORT(test_export);
	return 0;
}

int fini(){
	UNEXPORT(test_export);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = init,
	.fini = fini,
	.author = "tayoky",
	.name = "test",
	.description = "just a simple module to test",
	.license = "GPL 3",
};