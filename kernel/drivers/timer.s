section .text
extern kinfof
global timer
extern msg
extern irq_eoi
timer:
	mov rdi, msg
	call kinfof
	mov rdi, 0
	call irq_eoi
	iretq
section .data
msg:
db `timer trigger\n`, 0