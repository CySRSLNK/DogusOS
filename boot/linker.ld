/* DogusOS 内核链接脚本，入口为 boot.asm 中的 _start */
ENTRY(_start)

SECTIONS
{
	/* 内核加载地址 1MB，GRUB 从此处加载 ELF */
	. = 0x100000;

	.text : {
		*(.multiboot2)
		*(.text)
	}

	.rodata : {
		*(.rodata)
	}

	.data : {
		*(.data)
	}

	.bss : {
		*(COMMON)
		*(.bss)
	}

	/* 用户测试代码与栈，ring3 可执行，单独成段便于定位 */
	. = ALIGN(0x1000);
	_user_test_start = .;

	.user_test : {
		*(.user_test)
	}

	_kernel_end = .;
}