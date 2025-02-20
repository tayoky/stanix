#include "limine.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "kernel.h"
#include "asm.h"
#include "print.h"
#include "bitmap.h"
#include "paging.h"
#include "kheap.h"
#include "tmpfs.h"
#include "tarfs.h"
#include "devices.h"
#include "framebuffer.h"
#include "ini.h"
#include "terminal_emu.h"
#include "kout.h"
#include "irq.h"
#include "pit.h"
#include "string.h"
#include "tss.h"
#include "sys.h"
#include "exec.h"
#include "cmos.h"
#include "ps2.h"
#include <sys/time.h>

kernel_table master_kernel_table;
kernel_table *kernel;

struct timeval time = {
	.tv_sec = 0,
	.tv_usec = 0,
};

static void ls(const char *path){
	vfs_node *node = vfs_open(path,VFS_READONLY);

	struct dirent *ret;
	uint64_t index = 0;
	while(1){
		ret = vfs_readdir(node,index);
		if(!ret)break;
		kprintf("%s\n",ret->d_name);
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
		init_path = strdup("initrd:/init");
	}

	kinfof("try to exec %s\n",init_path);

	if(exec(init_path,0,NULL)){
		kfail();
		kinfof("can't spawn %s\n",init_path);
	}

	kfree(init_path);

	kinfof("halt because no init program can be executed\n");
	halt();
}

//the entry point
void kmain(){
	kernel = &master_kernel_table;
	disable_interrupt();
	init_serial();
	kinfof("starting stanix kernel\n");
	print_license();
	get_bootinfo();
	init_gdt();
	init_idt();
	enable_interrupt();
	init_tss();
	init_bitmap();
	kprintf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
	init_paging();
	init_kheap();
	init_vfs();
	init_tmpfs();
	mount_initrd();
	init_devices();
	init_frambuffer();
	read_main_conf_file();
	init_terminal_emualtor();
	init_kout();
	init_irq();
	init_task();
	init_ps2();
	init_cmos();
	init_pit();
	init_syscall();

	//spawn init
	ls("dev:/");
	kstatus("finish init kernel\n");
	spawn_init();

}