section .text
extern kinfof
global timer
extern msg
extern irq_eoi
extern sleep_tick
extern context_switch
extern time
timer:
	;push
	push rax
	push rbx

	;increase time
	mov rax, qword[time + 8]
	add rax, 1000
	cmp rax, 1000000
	jl skip
	xor rax, rax  ;reset subsecond to 0
	mov rbx, qword[time]
	inc rbx
	mov qword[time], rbx
	skip:
	mov qword[time + 8], rax

	;pop
	pop rbx
	pop rax

	jmp context_switch

	;push all
	push rdi
	push rsi
	push rax
	push rbx
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	push rbp

	;end of interrupt
	xor rdi, rdi
	call irq_eoi

	;pop all
	pop rbp
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rbx
	pop rax
	pop rsi
	pop rdi
	iretq