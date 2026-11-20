#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdint.h>

struct exception_frame {
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
	uint64_t vector;
	uint64_t error_code;
	uint64_t rip, cs, rflags, rsp, ss;
};

void exception_handler(struct exception_frame *frame);

#endif