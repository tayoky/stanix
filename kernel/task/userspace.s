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

	;setup argc and argv
	mov rdi, rdx ;argc
	mov rsi, rcx ;argv

	;clear registers
	xor rax, rax
	xor rbx, rbx
	xor rcx, rcx
	xor rdx, rdx
	xor r8, r8
	xor r9, r9
	xor r10, r10
	xor r11, r11
	xor r12, r12
	xor r13, r13
	xor r14, r14
	xor r15, r15

	;return
	iretq