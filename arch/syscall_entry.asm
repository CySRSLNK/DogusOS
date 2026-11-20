; syscall 指令入口，保存用户上下文，切内核栈，调 C 处理函数，iretq 返回
bits 64
global syscall_entry
extern syscall_handler
extern user_rsp_arr
extern current_task_idx
extern syscall_rsp
extern user_ctx_rip
extern user_ctx_rsp
extern user_ctx_rflags

section .text
syscall_entry:
	mov r14, rcx
	mov r15, r11
	mov r13, rax

	mov rbx, rdi
	mov rbp, rsi
	mov r12, rdx
	mov rcx, r10
	mov rdx, r8
	
	mov rax, [rel current_task_idx]
	lea rdi, [rel user_ctx_rip]
	mov [rdi + rax * 8], r14
	lea rdi, [rel user_ctx_rsp]
	mov [rdi + rax * 8], rsp
	lea rdi, [rel user_ctx_rflags]
	mov [rdi + rax * 8], r15
	lea rdi, [rel user_rsp_arr]
	mov [rdi + rax * 8], rsp

	lea rdi, [rel syscall_rsp]
	mov rsp, [rdi + rax * 8]

	push r14
	push r15
	push rbx
	push rbp
	push r12
	push r13

	mov r9, rdx
	mov r8, rcx
	mov rcx, r12
	mov rdx, rbp
	mov rsi, rbx
	mov rdi, r13

	call syscall_handler

	pop r13
	pop r12
	pop rbp
	pop rbx
	pop r15
	pop r14

	mov rcx, [rel current_task_idx]
	lea rdi, [rel user_rsp_arr]
	mov rdx, [rdi + rcx * 8]

	push qword 0x1B
	push rdx
	push r15
	push qword 0x23
	push r14
	iretq