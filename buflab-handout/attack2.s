	.file	"main.c"
	.text
	.globl	main
	.type	main, @function
main:
	mov $0x44d13e84, %eax
	mov %eax, 0x804b228
	push $0x08048bd9
	ret
	.size	main, .-main
	.ident	"GCC: (GNU) 9.2.0"
	.section	.note.GNU-stack,"",@progbits

