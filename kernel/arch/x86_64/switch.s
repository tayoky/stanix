extern kernel

;FIXME : race conditon if switch happend between start of this func
;and the cli
global context_switch
context_switch:
	;save eflags first
	pushfq
	cli
	pop rax
	mov [rdi + 512 + 8 * 21], rax

	mov rax, cr2
	mov [rdi + 512 + 8 * 0 ], rax
	;no need for cr3
	mov [rdi + 512 + 8 * 2], rax
	mov [rdi + 512 + 8 * 3 ], rbx
	mov [rdi + 512 + 8 * 4 ], rcx
	mov [rdi + 512 + 8 * 5 ], rdx
	mov [rdi + 512 + 8 * 6 ], rsi
	mov [rdi + 512 + 8 * 7 ], rdi
	mov [rdi + 512 + 8 * 8 ], rbp
	mov [rdi + 512 + 8 * 9 ], r8
	mov [rdi + 512 + 8 * 10], r9
	mov [rdi + 512 + 8 * 11], r10
	mov [rdi + 512 + 8 * 12], r11
	mov [rdi + 512 + 8 * 13], r12
	mov [rdi + 512 + 8 * 14], r13
	mov [rdi + 512 + 8 * 15], r14
	mov [rdi + 512 + 8 * 16], r15
	;return address
	pop rax
	mov [rdi + 512 + 8 * 19], rax
	mov [rdi + 512 + 8 * 20], cs
	;already saved flags
	mov [rdi + 512 + 8 * 22], rsp
	mov [rdi + 512 + 8 * 23], ss

	fxsave [rdi]

	;load new context
	;load rsi and flags/seg last
	fxrstor [rsi]
	
	mov rax, [rdi + 512 + 8 * 0 ]
	mov cr2, rax
	mov rax, [rsi + 512 + 8 * 2 ]
	mov rbx, [rsi + 512 + 8 * 3 ]
	mov rcx, [rsi + 512 + 8 * 4 ]
	mov rdx, [rsi + 512 + 8 * 5 ]
	;load rsi at the end
	mov rdi, [rsi + 512 + 8 * 7 ]
	mov rbp, [rsi + 512 + 8 * 8 ]
	mov r8 , [rsi + 512 + 8 * 9 ]
	mov r9 , [rsi + 512 + 8 * 10]
	mov r10, [rsi + 512 + 8 * 11]
	mov r11, [rsi + 512 + 8 * 12]
	mov r12, [rsi + 512 + 8 * 13]
	mov r13, [rsi + 512 + 8 * 14]
	mov r14, [rsi + 512 + 8 * 15]
	mov r15, [rsi + 512 + 8 * 16]

	mov rsp, rsi
	add rsp, 512 + 8 * 19

	;todo proper fs for tls
	mov ds, [rsi + 512 + 8 * 23]
	mov es, [rsi + 512 + 8 * 23]
	mov fs, [rsi + 512 + 8 * 23]
	mov gs, [rsi + 512 + 8 * 23]

	;we don't need rsi anymore
	;use ss since we loaded the new ds
	mov rsi, [ss:rsi + 512 + 8 * 6]

	iretq