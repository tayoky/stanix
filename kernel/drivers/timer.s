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

	;end of interrupt
	push rdi
	xor rdi, rdi
	call irq_eoi
	pop rdi

	iretq