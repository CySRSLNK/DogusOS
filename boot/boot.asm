; DogusOS 引导汇编，Multiboot 2 头部与 32 位到 64 位长模式切换
; GDT 含内核段、用户段与 TSS 描述符，TSS 描述符由 C 代码运行时填写
; 汇编器：NASM，输出 ELF64

MB2_MAGIC	equ 0xE85250D6
MB2_ARCH	equ 0
MB2_HEADER_LEN	equ multiboot2_header_end - multiboot2_header_start
MB2_CHECKSUM	equ -(MB2_MAGIC + MB2_ARCH + MB2_HEADER_LEN)

section .multiboot2
align 8
multiboot2_header_start:
	dd MB2_MAGIC
	dd MB2_ARCH
	dd MB2_HEADER_LEN
	dd MB2_CHECKSUM

	; 帧缓冲信息请求标签，宽高深全部写 0，由 GRUB 按 gfxmode 决定
align 8
framebuffer_tag_start:
	dw 5
	dw 0
	dd framebuffer_tag_end - framebuffer_tag_start
	dd 0
	dd 0
	dd 0
framebuffer_tag_end:

	; 结束标签
align 8
end_tag_start:
	dw 0
	dw 0
	dd end_tag_start - end_tag_start + 8
end_tag_start_end:
multiboot2_header_end:

section .bss
align 4096
global pml4_table
pml4_table:
	resb 4096
global pdpt_table
pdpt_table:
	resb 4096 * 4
global pd_table
pd_table:
	resb 4096 * 4

align 16
global stack_bottom
stack_bottom:
	resb 16384
global stack_top
stack_top:

section .data
align 8
global gdt64
gdt64:
	dq 0x0000000000000000		; 0x00 null
	dq 0x00209A0000000000		; 0x08 内核代码，DPL 0
	dq 0x0000920000000000		; 0x10 内核数据，DPL 0
	dq 0x00CFF2000000FFFF		; 0x18 用户数据，DPL 3
	dq 0x00AFFA000000FFFF		; 0x20 用户代码，DPL 3
	dq 0x0000000000000000		; 0x28 TSS 低 8 字节，运行时填
	dq 0x0000000000000000		; 0x30 TSS 高 8 字节，运行时填

gdt64_pointer:
	dw gdt64_pointer - gdt64 - 1
	dq gdt64

section .text
bits 32
global _start
extern kernel_main

_start:
	cli
	cld
	mov edi, ebx
	call setup_page_tables
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax
	mov eax, pml4_table
	mov cr3, eax
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr
	mov eax, cr0
	or eax, 1 << 31
	or eax, 1 << 0
	mov cr0, eax
	lgdt [gdt64_pointer]
	jmp 0x08:long_mode_entry

setup_page_tables:
	mov eax, pdpt_table
	or eax, 0x03
	mov [pml4_table], eax
	mov dword [pml4_table + 4], 0

	mov ecx, 0
.fill_pdpt:
	mov eax, ecx
	shl eax, 12
	add eax, pd_table
	or eax, 0x03
	mov [pdpt_table + ecx * 8], eax
	mov dword [pdpt_table + ecx * 8 + 4], 0
	inc ecx
	cmp ecx, 4
	jl .fill_pdpt

	mov ecx, 0
.fill_pd:
	mov eax, ecx
	shl eax, 21
	or eax, 0x83
	mov [pd_table + ecx * 8], eax
	mov dword [pd_table + ecx * 8 + 4], 0
	inc ecx
	cmp ecx, 2048
	jl .fill_pd

	ret

bits 64
long_mode_entry:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov rsp, stack_top
	mov rbp, rsp
	call kernel_main

.hang:
	hlt
	jmp .hang

; PIT 中断入口，向量 32
global isr32
extern timer_handler

isr32:
	push rax
	push rcx
	push rdx
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11

	call timer_handler

	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rax

	iretq

; 键盘中断入口，向量 33
global isr33
extern keyboard_handler

isr33:
	push rax
	push rcx
	push rdx
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11

	call keyboard_handler

	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rax

	iretq