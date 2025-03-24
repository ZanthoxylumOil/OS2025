# TODO: This is lab1.1
/* Real Mode Hello World */
.code16

.global start
start:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0x7d00, %ax
	movw %ax, %sp # setting stack pointer to 0x7d00
	# TODO:通过中断输出Hello World, 并且间隔1000ms打印新的一行

	# 输出
	movw $message, %bp 
        movw $14, %cx
        movw $0x1301, %ax
        movw $0x000c, %bx
        movw $0x0000, %dx
loop:
	# 中断
        int $0x10

        pushw %ax
        pushw %cx
        pushw %dx

	# 间隔1000ms
        movw $0x8600, %ax
        movw $0x000F, %cx
        movw $0x4240, %dx
        int $0x15

        pop %dx
        pop %cx
        pop %ax

	# 新的一行
        addw $0x0100, %dx

	jmp loop

message:
	.string "Hello, World!\n\0"







