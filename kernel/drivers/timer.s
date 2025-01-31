section .text
extern kinfof
global timer
extern msg
extern irq_eoi
timer:
	cli
	mov rdi, msg
	call kinfof
	mov rdi, 0
	call irq_eoi
	sti
	iretq
section .data
msg:
db `hello world from pit\n`, 0