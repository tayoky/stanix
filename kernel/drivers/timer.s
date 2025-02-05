section .text
extern kinfof
global timer
extern msg
extern irq_eoi
extern sleep_tick
extern context_switch
timer:
	
	;push
	push rax
	push rbx

	;dec sleep_tick if superior to 0
	mov rbx, sleep_tick
	mov rax, qword[rbx]
	cmp rax, 0
	je skip
	dec rax
	mov qword[rbx], rax
	skip:

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