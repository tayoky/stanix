global safe_copy_to
global safe_copy_from
global safe_copy_fault
global safe_copy_resolve_fault

; on x86_64 safe_copy_to and safe_copy_from can be implemented in a single func
; the idea is simple, we do a memcpy and we catch page fault in isr_handler(idt.c)
; we then return to safe_copy_fault_resolve and set rax to -EFAULT

safe_copy_to:
safe_copy_from:
	mov rcx, rdx
safe_copy_fault:
	rep movsb
	xor rax, rax
safe_copy_resolve_fault:
	ret