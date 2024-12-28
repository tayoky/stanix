#include "limine.h"
#include "serial.h"
#include "gdt.h"
#include "kernel.h"
#include "asm.h"

kernel_table master_kernel_table;

//the entry point
void kmain(){
        init_serial();
        init_gdt(&master_kernel_table);
        write_serial("hello world\n");
        //infinite loop
        while (1){
                
        }
}