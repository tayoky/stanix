;please make sure intterupt flags is clear when entering this shit

global context_switch
context_switch:
	mov [rdi + 512 + 8 * 0], gs
	mov [rdi + 512 + 8 * 1], fs
	mov [rdi + 512 + 8 * 2], es
	mov [rdi + 512 + 8 * 3], ds
	;save cr2
	mov rax, cr2
	mov [rdi + 512 + 8 * 4 ], rax
	;no need for cr3
	mov [rdi + 512 + 8 * 6], rax
	mov [rdi + 512 + 8 * 7 ], rbx
	mov [rdi + 512 + 8 * 8 ], rcx
	mov [rdi + 512 + 8 * 9 ], rdx
	mov [rdi + 512 + 8 * 10], rsi
	mov [rdi + 512 + 8 * 11], rdi
	mov [rdi + 512 + 8 * 12], rbp
	mov [rdi + 512 + 8 * 13], r8
	mov [rdi + 512 + 8 * 14], r9
	mov [rdi + 512 + 8 * 15], r10
	mov [rdi + 512 + 8 * 16], r11
	mov [rdi + 512 + 8 * 17], r12
	mov [rdi + 512 + 8 * 18], r13
	mov [rdi + 512 + 8 * 19], r14
	mov [rdi + 512 + 8 * 20], r15
	;return address
	pop rax
	mov [rdi + 512 + 8 * 23], rax
	mov [rdi + 512 + 8 * 24], cs
	;save flags
	pushfq
	pop rax
	mov [rdi + 512 + 8 * 25], rax
	mov [rdi + 512 + 8 * 26], rsp
	mov [rdi + 512 + 8 * 27], ss

	fxsave [rdi]

	;load new context
	;load rsi and flags/seg last
	fxrstor [rsi]
	
	mov rax, [rsi + 512 + 8 * 4 ]
	mov cr2, rax
	mov rax, [rsi + 512 + 8 * 6 ]
	mov rbx, [rsi + 512 + 8 * 7 ]
	mov rcx, [rsi + 512 + 8 * 8 ]
	mov rdx, [rsi + 512 + 8 * 9 ]
	;load rsi at the end
	mov rdi, [rsi + 512 + 8 * 11]
	mov rbp, [rsi + 512 + 8 * 12]
	mov r8 , [rsi + 512 + 8 * 13]
	mov r9 , [rsi + 512 + 8 * 14]
	mov r10, [rsi + 512 + 8 * 15]
	mov r11, [rsi + 512 + 8 * 16]
	mov r12, [rsi + 512 + 8 * 17]
	mov r13, [rsi + 512 + 8 * 18]
	mov r14, [rsi + 512 + 8 * 19]
	mov r15, [rsi + 512 + 8 * 20]

	mov rsp, rsi
	add rsp, 512 + 8 * 21

	mov gs, [rsi + 512 + 8 * 0]
	mov fs, [rsi + 512 + 8 * 1]
	mov es, [rsi + 512 + 8 * 2]
	mov ds, [rsi + 512 + 8 * 3]

	;we don't need rsi anymore
	;use ss since we loaded the new ds
	mov rsi, [ss:rsi + 512 + 8 * 10]

	iretq