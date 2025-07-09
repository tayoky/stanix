.text

.globl _start
_start:
	#write
	movq $4, %rax
	movq $1, %rdi
	movq $hello, %rsi
	movq $12, %rdx

	#exit
	int $0x80
	movq $0, %rax
	movq $0, %rdi
	int $0x80

.data
hello:
.string "hello world\n"
