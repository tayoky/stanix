global jump_userspace
jump_userspace:
	xor rax, rax

	;set the segment registers
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	;calculate the stack pointer
	sub rsi, 8

	;setup the stack frame
	push rax       ;ss
	push rsi   ; stack pointer
	pushf          ;flags
	push 0x08      ;cs
	push rdi       ;return address
	
	;setup rbp
	mov rbp, rsi

	;return
	iretq