global arch_registers_save
arch_registers_save:
	mov [rdi + 8 * 0], gs
	mov [rdi + 8 * 1], fs
	mov [rdi + 8 * 2], es
	mov [rdi + 8 * 3], ds
	;save cr2
	mov rax, cr2
	mov [rdi + 8 * 4 ], rax
	;no need for cr3
	mov qword [rdi + 8 * 6], 1 ;we want to return 1 the second time
	mov [rdi + 8 * 7 ], rbx
	mov [rdi + 8 * 8 ], rcx
	mov [rdi + 8 * 9 ], rdx
	mov [rdi + 8 * 10], rsi
	mov [rdi + 8 * 11], rdi
	mov [rdi + 8 * 12], rbp
	mov [rdi + 8 * 13], r8
	mov [rdi + 8 * 14], r9
	mov [rdi + 8 * 15], r10
	mov [rdi + 8 * 16], r11
	mov [rdi + 8 * 17], r12
	mov [rdi + 8 * 18], r13
	mov [rdi + 8 * 19], r14
	mov [rdi + 8 * 20], r15
	;return address
	pop rax
	mov [rdi + 8 * 23], rax
	mov [rdi + 8 * 26], rsp
	push rax
	mov [rdi + 8 * 24], cs
	;save flags
	pushfq
	pop rax
	mov [rdi + 8 * 25], rax
	mov [rdi + 8 * 27], ss

	mov rax, 0
	ret

global arch_registers_load
arch_registers_load:
	;load rdi and flags/seg last

	mov rax, [rdi + 8 * 4 ]
	mov cr2, rax
	mov rax, [rdi + 8 * 6 ]
	mov rbx, [rdi + 8 * 7 ]
	mov rcx, [rdi + 8 * 8 ]
	mov rdx, [rdi + 8 * 9 ]
	mov rsi, [rdi + 8 * 10]
	;load rdi at the end
	mov rbp, [rdi + 8 * 12]
	mov r8 , [rdi + 8 * 13]
	mov r9 , [rdi + 8 * 14]
	mov r10, [rdi + 8 * 15]
	mov r11, [rdi + 8 * 16]
	mov r12, [rdi + 8 * 17]
	mov r13, [rdi + 8 * 18]
	mov r14, [rdi + 8 * 19]
	mov r15, [rdi + 8 * 20]

	mov rsp, rdi
	add rsp, 512 + 8 * 23

	mov gs, [rdi + 8 * 0]
	mov es, [rdi + 8 * 2]
	mov ds, [rdi + 8 * 3]

	;we don't need rdi anymore
	;this is fucked
	mov rdi, [rdi + 8 * 11]

	iretq
