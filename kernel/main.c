#include "limine.h"
#include "serial.h"

//the entry point
void kmain(){
        init_serial();
        write_serial("hello world");
        //infinite loop
        while (1){
                
        }
}