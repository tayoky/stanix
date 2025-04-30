extern kernel
global context_switch
context_switch:

	;save context
	;use cr2 to store the address to return
	pop eax
	mov cr2, eax

	;save ss
	xor eax, eax
	mov ax, ss
	push eax ;ss

	mov eax, esp
	add eax, 8
	push eax
	pushf

	cli

	;save cs
	xor eax, eax
	mov ax, cs
	push eax ;cs

	;save rip
	mov eax, cr2
	push eax ;rip

	push 0
	push 0
	push ebp
	push edi
	push esi
	push edx
	push ecx
	push ebx
	push eax
	push 0 ;cr3
	push 0 ;cr2

	;first save rsp

	mov dword[edi + 8], esp
	mov eax, dword[esi]
	mov cr3,eax
	mov esp, dword[esi + 8]

	add esp, 16 ;skip cr2 ad cr3
	pop eax
	pop ebx
	pop ecx
	pop edx
	pop esi
	pop edi
	pop ebp
	add esp, 16 ;skip err code and type

	;restore ds and other segment
	push eax
	mov eax, dword[esp + 40]

	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	pop eax
	
	iret