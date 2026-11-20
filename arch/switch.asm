; 上下文切换
bits 64
section .text

global switch_to
switch_to:
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	mov [rdi], rsp
	mov rax, [rsi + 8]
	mov cr3, rax
	mov rsp, [rsi]
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

global switch_to_user
switch_to_user:
	cli
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	mov [rdi], rsp
	mov rax, [rsi + 8]
	mov cr3, rax

	mov rax, [rsi + 24]
	mov rcx, [rsi + 32]
	mov rsp, rcx
	mov rdx, [rsi + 40]

	push qword 0x1B
	push rcx
	push rdx
	push qword 0x23
	push rax

	xor rax, rax
	iretq

global fork_trampoline
fork_trampoline:
	xor eax, eax
	sti
	ret