global jump_userspace
jump_userspace:
	xor eax, eax
	mov ax, 0x23

	;setup the stack frame
	push eax       ;ss
	push esi       ;stack pointer
	pushf          ;flags
	push 0x1B      ;cs
	push edi       ;return address
	
	;setup ebp
	mov ebp, esi

	;setup argc and argv
	mov edi, edx ;argc
	mov esi, ecx ;argv

	;TODO i d'ont now how this is supposed to work
	;mov rdx, r8 ;envc
	;mov rcx, r9 ;envp

	;clear registers
	xor ebx, ebx
	
	
	;set the segment registers
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	xor eax, eax

	;return
	iret