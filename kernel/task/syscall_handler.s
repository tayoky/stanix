extern sys_exit
extern sys_open
extern sys_close
extern sys_read
extern sys_write
extern sys_getpid
extern kdebugf
section .data
syscall_table:
dq sys_exit
dq sys_open
dq sys_close
dq sys_read
dq sys_write
dq sys_getpid
syscall_table_end:
msg:
db `rax : %lx\n`, 0
section .text
global syscall_handler
syscall_handler:
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	mov bx, ds
	push rbx

	;restore kernel segment
	mov bx, 0x10
	mov ds, bx
	mov es, bx
	mov fs ,bx
	mov gs, bx

	;first out of bound check
	cmp rax, (syscall_table_end - syscall_table) / 8
	jae sys_invalid

	;mov rdi, msg
	;mov rsi, qword[syscall_table + rax * 8]
	;call kdebugf

	;now just call the handler
	call qword[syscall_table + rax * 8]

	;restore user segment
	pop rdx
	mov ds, dx
	mov es, dx
	mov fs ,dx
	mov gs, dx
	
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	iretq

sys_invalid:
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	mov rax, -1
	iretq