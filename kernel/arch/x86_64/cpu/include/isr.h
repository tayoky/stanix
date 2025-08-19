#ifndef ISR_H
#define ISR_H

void divide_exception();
void overflow_exception();
void invalid_op_exception();
void invalid_tss_exception();
void global_fault_exception();
void pagefault_exception();
void isr_ignore();
void isr128();
#endif