#include <kernel/limine.h>
#include <kernel/serial.h>
#include <kernel/kernel.h>
#include <kernel/asm.h>
#include <kernel/print.h>
#include <kernel/pmm.h>
#include <kernel/paging.h>
#include <kernel/kheap.h>
#include <kernel/tmpfs.h>
#include <kernel/tarfs.h>
#include <kernel/devices.h>
#include <kernel/framebuffer.h>
#include <kernel/ini.h>
#include <kernel/terminal_emu.h>
#include <kernel/kout.h>
#include <kernel/irq.h>
#include <kernel/string.h>
#include <kernel/sys.h>
#include <kernel/exec.h>
#include <kernel/module.h>
#include <kernel/proc.h>
#include <kernel/sysfs.h>
#include <sys/time.h>

kernel_table master_kernel_table;
kernel_table *kernel = &master_kernel_table;

struct timeval time = {
	.tv_sec = 0,
	.tv_usec = 0,
};

static void ls(const char *path){
	vfs_node *node = vfs_open(path,VFS_READONLY);
	if(!node){
		kprintf("%s don't exist\n",path);
		return;
	}

	struct dirent *ret;
	uint64_t index = 0;
	while(1){
		ret = vfs_readdir(node,index);
		if(!ret)break;
		kprintf("%s\n",ret->d_name);
		kfree(ret);
		index++;
	}

	vfs_close(node);
}

void print_license(void){
	kinfof("the STANIX kernel\n"
		"Copyright (C) 2025  tayoky\n"
		"\n"
		"This program is free software: you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation, either version 3 of the License, or\n"
		"(at your option) any later version.\n"
		"\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"GNU General Public License for more details.\n");
}

void spawn_init(){
	kstatus("try spawn init...\n");
	//first get the path for the init program
	char *init_path = ini_get_value(kernel->conf_file,"init","init");

	if(!init_path){
		init_path = strdup("/init");
	}

	kinfof("try to exec %s\n",init_path);

	const char *arg[] = {
		init_path,
		NULL
	};

	const char *env[] = {
		"KERNEL=stanix",
		NULL
	};

	if(exec(init_path,1,arg,1,env)){
		kfail();
		kinfof("can't spawn %s\n",init_path);
	}

	kfree(init_path);

	kinfof("halt because no init program can be executed\n");
	halt();
}

//the entry point
void kmain(){
	enable_interrupt();
	kinfof("\n"
		"███████╗████████╗ █████╗ ███╗   ██╗██╗██╗  ██╗    \n"
		"██╔════╝╚══██╔══╝██╔══██╗████╗  ██║██║╚██╗██╔╝    \n"
		"███████╗   ██║   ███████║██╔██╗ ██║██║ ╚███╔╝     \n"
		"╚════██║   ██║   ██╔══██║██║╚██╗██║██║ ██╔██╗     \n"
		"███████║   ██║   ██║  ██║██║ ╚████║██║██╔╝ ██╗    \n"
		"╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝    \n"
		"                                                  \n"
		"██╗  ██╗███████╗██████╗ ███╗   ██╗███████╗██╗     \n"
		"██║ ██╔╝██╔════╝██╔══██╗████╗  ██║██╔════╝██║     \n"
		"█████╔╝ █████╗  ██████╔╝██╔██╗ ██║█████╗  ██║     \n"
		"██╔═██╗ ██╔══╝  ██╔══██╗██║╚██╗██║██╔══╝  ██║     \n"
		"██║  ██╗███████╗██║  ██║██║ ╚████║███████╗███████╗\n"
		"╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚══════╝╚══════╝\n"
	);
	print_license();
	get_bootinfo();
	init_PMM();
	kprintf("used pages: 0x%lx\n",master_kernel_table.used_memory/PAGE_SIZE);
	init_paging();
	init_kheap();
	init_vfs();
	mount_initrd();
	init_tmpfs();
	init_devices();
	init_frambuffer();
	read_main_conf_file();
	init_terminal_emualtor();
	init_kout();
	init_irq();
	init_task();
	init_timer();
	init_mod();
	init_proc();
	init_sysfs();
	
	//just to debug
	ls("/dev/");

	kstatus("finish init kernel\n");
	spawn_init();

}