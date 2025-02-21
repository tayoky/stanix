global jump_userspace
jump_userspace:
	xor rax, rax

	;set the segment registers
	mov ax, 0x23
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	;setup the stack frame
	push rax       ;ss
	push rsi       ;stack pointer
	pushf          ;flags
	push 0x1B      ;cs
	push rdi       ;return address
	
	;setup rbp
	mov rbp, rsi

	;return
	iretq