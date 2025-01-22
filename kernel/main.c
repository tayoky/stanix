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

kernel_table master_kernel_table;
kernel_table *kernel;

//the entry point
void kmain(){
        kernel = &master_kernel_table;
        disable_interrupt();
        init_serial();
        kinfof("starting stanix kernel\n");
        get_bootinfo();
        init_gdt();
        init_idt();
        enable_interrupt();
        init_bitmap();
        kprintf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        init_paging();
        init_kheap();
        init_vfs();
        init_tmpfs();
        mount_initrd();
        init_devices();
        kstatus("finish init kernel\n");

        //just a test to test all PMM and paging functionality
        kdebugf("test mapping/unmapping\n");
        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("create new PMLT4\n");
        uint64_t *PMLT4 = init_PMLT4(&master_kernel_table);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("allocate page and map it\n");
        uint64_t test_page = allocate_page(&master_kernel_table.bitmap);
        map_page(PMLT4,test_page,0xFFFFFFFF/PAGE_SIZE,PAGING_FLAG_RW_CPL0);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("unmapping page\n");
        unmap_page(PMLT4,0xFFFFFFFF/PAGE_SIZE);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("free page\n");
        free_page(&master_kernel_table.bitmap,test_page);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("delete PMLT4\n");
        delete_PMLT4(PMLT4);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);

        kdebugf("alloc test\n");
        kdebugf("alloc 128 bytes\n");
        uint64_t *test_ptr = kmalloc(128);
        kdebugf("allocated at 0x%lx\n",test_ptr);
        kfree(test_ptr);
        kdebugf("free succefully\n");
        
        
        //infinite loop
        kprintf("test V2P : 0x%lx\n",virt2phys((void *)master_kernel_table.kernel_address->virtual_base));
        halt();
}