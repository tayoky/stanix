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

kernel_table master_kernel_table;

//the entry point
void kmain(){
        disable_interrupt();
        init_serial();
        kinfof("starting stanix kernel\n");
        get_bootinfo(&master_kernel_table);
        init_gdt(&master_kernel_table);
        init_idt(&master_kernel_table);
        enable_interrupt();
        init_bitmap(&master_kernel_table);
        kprintf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        init_paging(&master_kernel_table);
        init_kheap(&master_kernel_table);
        kstatus("finish init kernel\n");

        //just a test to test all PMM and paging functionality
        kdebugf("test mapping/unmapping\n");
        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("create new PMLT4\n");
        uint64_t *PMLT4 = init_PMLT4(&master_kernel_table);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("allocate page and map it\n");
        uint64_t test_page = allocate_page(&master_kernel_table.bitmap);
        map_page(&master_kernel_table,PMLT4,test_page,0xFFFFFFFF/PAGE_SIZE,PAGING_FLAG_RW_CPL0);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("unmapping page\n");
        unmap_page(&master_kernel_table,PMLT4,0xFFFFFFFF/PAGE_SIZE);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("free page\n");
        free_page(&master_kernel_table.bitmap,test_page);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);
        kdebugf("delete PMLT4\n");
        delete_PMLT4(&master_kernel_table,PMLT4);

        kdebugf("used pages: 0x%lx\n",master_kernel_table.bitmap.used_page_count);

        kdebugf("alloc test\n");
        kdebugf("alloc 128 bytes\n");
        uint64_t *test_ptr = kmalloc(&master_kernel_table,128);
        kdebugf("allocated at 0x%lx\n",test_ptr);
        kfree(&master_kernel_table,test_ptr);
        kdebugf("free succefully\n");
        test_ptr = kmalloc(&master_kernel_table,256);
        kdebugf("allocated 256 bytes at 0x%lx\n",test_ptr);

        uint8_t *tests_ptrs[128];
        kdebugf("stress test allocating 256 bytes 128 times and then free it\n");
        for (uint8_t i = 0; i < 256; i++){
                tests_ptrs[i] = kmalloc(&master_kernel_table,256);
                kdebugf("%d : allocating at 0x%lx\n",i,tests_ptrs[i]);
        }
        
        
        //infinite loop
        kprintf("test V2P : 0x%lx\n",virt2phys(&master_kernel_table,master_kernel_table.kernel_address->virtual_base));
        halt();
}