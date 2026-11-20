; 异常入口，0~31 号异常统一压栈后调用 C 处理函数
; 无 error code 的向量手工压 0，保证栈帧统一
bits 64

section .text

extern exception_handler

%macro isr_no_err 1
global isr%1
isr%1:
	push qword 0
	push qword %1
	jmp isr_common
%endmacro

%macro isr_err 1
global isr%1
isr%1:
	push qword %1
	jmp isr_common
%endmacro

isr_no_err 0		; #DE 除零
isr_no_err 1		; #DB 调试
isr_no_err 2		; NMI 不可屏蔽中断
isr_no_err 3		; #BP 断点
isr_no_err 4		; #OF 溢出
isr_no_err 5		; #BR 越界
isr_no_err 6		; #UD 非法指令
isr_no_err 7		; #NM 设备不可用
isr_err 8		; #DF 双重故障
isr_no_err 9		; 协处理器段超限，保留
isr_err 10		; #TS 无效 TSS
isr_err 11		; #NP 段不存在
isr_err 12		; #SS 栈段错误
isr_err 13		; #GP 通用保护
isr_err 14		; #PF 缺页
isr_no_err 15		; 保留
isr_no_err 16		; #MF x87 浮点
isr_err 17		; #AC 对齐检查
isr_no_err 18		; #MC 机器检查
isr_no_err 19		; #XM SIMD 浮点
isr_no_err 20		; #VE 虚拟化异常
isr_err 21		; #CP 控制保护
isr_no_err 22		; 保留
isr_no_err 23		; 保留
isr_no_err 24		; 保留
isr_no_err 25		; 保留
isr_no_err 26		; 保留
isr_no_err 27		; 保留
isr_no_err 28		; #HV 虚拟化
isr_err 29		; #VC VMM 通信
isr_err 30		; #SX 安全异常
isr_no_err 31		; 保留

isr_common:
	push rax
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	mov rdi, rsp
	call exception_handler

exception_hang:
	hlt
	jmp exception_hang