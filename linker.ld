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
}