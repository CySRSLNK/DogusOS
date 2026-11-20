// ring3 测试，验证 iretq 进用户态、syscall 往返、sysret 返回
// 1c 阶段不验证 exit 路径，用户代码打印后停在 ring3 循环
#include <stdint.h>
#include "kernel.h"
#include "tss.h"
#include "syscall.h"
#include "user_test.h"

extern char _user_test_start;

static uint8_t user_stack[16384];
static uint8_t syscall_kstack[16384];

__attribute__((section(".user_test")))
static void user_test_entry(void) {
	__asm__ volatile (
		"mov $1, %%rax\n\t"
		"mov $1, %%rdi\n\t"
		"lea 1f(%%rip), %%rsi\n\t"
		"mov $19, %%rdx\n\t"
		"syscall\n\t"
		"2: jmp 2b\n\t"
		"1: .ascii \"hello from ring3!!\\n\"\n\t"
		:
		:
		: "memory"
	);
}

static void mark_user_page(uint64_t addr) {
	uint64_t pml4_idx = (addr >> 39) & 0x1FF;
	uint64_t pdpt_idx = (addr >> 30) & 0x1FF;
	uint64_t pd_idx = (addr >> 21) & 0x1FF;

	extern uint64_t pml4_table[];
	extern uint64_t pdpt_table[];
	extern uint64_t pd_table[];

	pml4_table[pml4_idx] |= 0x04;
	pdpt_table[pdpt_idx] |= 0x04;
	pd_table[pd_idx] |= 0x04;

	__asm__ volatile ("mov %%cr3, %%rax\n\tmov %%rax, %%cr3" : : : "rax");
}

static void enter_ring3(uint64_t entry, uint64_t ustack) {
	__asm__ volatile (
		"push $0x1B\n\t"
		"push %0\n\t"
		"push $0x202\n\t"
		"push $0x23\n\t"
		"push %1\n\t"
		"iretq\n\t"
		:
		: "r"(ustack), "r"(entry)
		: "memory"
	);
}

void user_test_run(void) {
	mark_user_page((uint64_t)(uintptr_t)&_user_test_start);
	mark_user_page((uint64_t)(uintptr_t)(&_user_test_start + 0x1000));
	mark_user_page((uint64_t)(uintptr_t)user_stack);
	mark_user_page((uint64_t)(uintptr_t)(user_stack + sizeof(user_stack) - 1));

	uint64_t kstack = (uint64_t)(uintptr_t)(syscall_kstack + sizeof(syscall_kstack));

	kstack &= ~0xFULL;
	tss_set_rsp0(kstack);

	extern uint64_t kernel_rsp_save;
	kernel_rsp_save = kstack;

	__asm__ volatile ("cli");

	enter_ring3((uint64_t)(uintptr_t)&user_test_entry,
		    (uint64_t)(uintptr_t)(user_stack + sizeof(user_stack)));
}