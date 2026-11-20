// 系统调用分发表与实现
#include <stdint.h>
#include "kernel.h"
#include "syscall.h"
#include "syscall_entry.h"
#include "task.h"

uint64_t exit_resume_rip;
uint64_t exit_resume_rsp;
uint64_t exit_resume_rbp;

uint64_t user_rsp_save;
uint64_t kernel_rsp_save;

static void wrmsr(uint32_t msr, uint64_t val) {
	uint32_t lo = (uint32_t)(val & 0xFFFFFFFF);
	uint32_t hi = (uint32_t)(val >> 32);

	__asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static uint64_t rdmsr(uint32_t msr) {
	uint32_t lo;
	uint32_t hi;

	__asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((uint64_t)hi << 32) | lo;
}

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count) {
	ctrl_c_hit = 0;

	if (fd != 1) {
		return -1;
	}

	const char *p = (const char *)(uintptr_t)buf;

	for (uint64_t i = 0; i < count; i++) {
		print_char(p[i]);
	}

	return (int64_t)count;
}

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
	(void)a4;
	(void)a5;

	if (num == SYS_WRITE) {
		return (uint64_t)sys_write(a1, a2, a3);
	}

	if (num == SYS_GETPID) {
		return (uint64_t)task_getpid();
	}

	if (num == SYS_FORK) {
		return (uint64_t)task_fork();
	}

	if (num == SYS_EXIT) {
		task_exit((int)a1);
		return 0;
	}

	if (num == SYS_WAIT) {
		int code = 0;
		int pid = task_wait(&code);

		if (pid > 0 && a1 != 0) {
			*(int *)(uintptr_t)a1 = code;
		}

		return (uint64_t)pid;
	}

	if (num == SYS_SLEEP) {
		task_sleep((uint32_t)a1);
		return 0;
	}

	return (uint64_t)-1;
}

void syscall_init(void) {
	uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
	uint64_t lstar = (uint64_t)(uintptr_t)syscall_entry;
	uint64_t fmask = 0x47700;

	wrmsr(0xC0000081, star);
	wrmsr(0xC0000082, lstar);
	wrmsr(0xC0000084, fmask);

	uint64_t efer = rdmsr(0xC0000080);

	efer |= 1;
	wrmsr(0xC0000080, efer);
}