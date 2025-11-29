global jump_userspace
jump_userspace:
	xor rax, rax
	mov ax, 0x23

	;setup the stack frame
	push rax       ;ss
	push rsi       ;stack pointer
	pushf          ;flags
	push 0x1B      ;cs
	push rdi       ;return address

	;setup argc and argv
	mov rdi, rdx ;argc
	mov rsi, rcx ;argv

	mov rdx, r8 ;envc
	mov rcx, r9 ;envp

	;clear registers
	xor rbx, rbx
	xor r8, r8
	xor r9, r9
	xor r10, r10
	xor r11, r11
	xor r12, r12
	xor r13, r13
	xor r14, r14
	xor r15, r15
	xor rbp, rbp
	
	
	;set the segment registers
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	xor rax, rax

	;return
	iretq
	