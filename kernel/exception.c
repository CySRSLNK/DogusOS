// 异常处理，打印向量名、错误码、寄存器快照后停机
#include <stdint.h>
#include "kernel.h"
#include "exception.h"

static const char *exception_names[32] = {
	"Divide Error",
	"Debug",
	"Non-maskable Interrupt",
	"Breakpoint",
	"Overflow",
	"Bound Range Exceeded",
	"Invalid Opcode",
	"Device Not Available",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Invalid TSS",
	"Segment Not Present",
	"Stack-Segment Fault",
	"General Protection Fault",
	"Page Fault",
	"Reserved",
	"x87 Floating-Point",
	"Alignment Check",
	"Machine Check",
	"SIMD Floating-Point",
	"Virtualization Exception",
	"Control Protection",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Hypervisor Injection",
	"VMM Communication",
	"Security Exception",
	"Reserved"
};

void exception_handler(struct exception_frame *frame) {
	uint64_t v = frame->vector;

	if (v >= 32) {
		v = 31;
	}

	print("\n=== Exception ===\n");

	print("Vector: ");
	print_uint((uint32_t)frame->vector);
	print(" (");
	print(exception_names[v]);
	print(")\n");

	print("Error:  ");
	print_hex(frame->error_code);
	print_char('\n');

	print("RIP:    ");
	print_hex(frame->rip);
	print_char('\n');

	print("CS:     ");
	print_hex(frame->cs);
	print_char('\n');

	print("RFLAGS: ");
	print_hex(frame->rflags);
	print_char('\n');

	print("RSP:    ");
	print_hex(frame->rsp);
	print_char('\n');

	print("SS:     ");
	print_hex(frame->ss);
	print_char('\n');

	print("=================\n");

	for (;;) {
		__asm__ volatile ("hlt");
	}
}