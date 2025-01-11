#include "limine.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "kernel.h"
#include "asm.h"
#include "print.h"
#include "bitmap.h"
#include "paging.h"

kernel_table master_kernel_table;

//the entry point
void kmain(){
        disable_interrupt();
        init_serial();
        get_bootinfo(&master_kernel_table);
        init_gdt(&master_kernel_table);
        init_idt(&master_kernel_table);
        enable_interrupt();
        init_bitmap(&master_kernel_table);
        kprintf("finish init kernel\n");
        
        //infinite loop
        kprintf("test V2P : 0x%lx\n",virt2phys(&master_kernel_table,master_kernel_table.bootinfo.kernel_address_response->virtual_base));
        halt();
}