global context_switch
extern schedule
extern get_current_proc
extern irq_eoi

context_switch:
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

	;push seg
	mov ax, ds
	push rax

	call get_current_proc
	;the current proc is in rax

	;save rsp
	mov qword[rax + 8], rsp 

	call schedule

	call get_current_proc

	;now reload cr3 and rsp
	mov rbx, qword[rax]
	mov cr3, rbx
	mov rsp, qword[rax + 8]

	;end of interrupt
	push rdi
	xor rdi, rdi
	call irq_eoi
	pop rdi

	;pop seg
	pop rax

	;little tick : ds,es,fs and gs are alaways the same
	;so only save ds
	;cs and ss are aready saved by the intterrupt frame
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs,ax

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