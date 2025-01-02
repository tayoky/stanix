#ifndef INTERRUPT_H
#define INTERRUPT_H

void divide_exception();
void overflow_exception();
void invalid_op_exception();
void invalid_tss_exception();
void global_fault_exception();
void pagefault_exception();
#endif