#include "limine.h"
#include "serial.h"

//the entry point
void kmain(){
        init_serial();
        write_serial_char("hello world");
        //infinite loop
        while (1){
                
        }
}