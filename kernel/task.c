// 内核任务与抢占调度，支持内核态和 ring3 用户态任务
#include <stdint.h>
#include "kernel.h"
#include "task.h"
#include "vmm.h"
#include "pmm.h"
#include "tss.h"

#define INVALID_PID	0xFFFFFFFFu

struct task tasks[TASK_MAX];
int task_count = 0;
int current_task_idx = 0;
uint64_t syscall_rsp[TASK_MAX];
uint64_t user_rsp_arr[TASK_MAX];
uint64_t user_ctx_rip[TASK_MAX];
uint64_t user_ctx_rsp[TASK_MAX];
uint64_t user_ctx_rflags[TASK_MAX];

static uint8_t task_stacks[TASK_MAX][TASK_STACK_SIZE]
	__attribute__((aligned(4096)));

static uint8_t user_stacks[TASK_MAX][USER_STACK_SIZE]
	__attribute__((aligned(4096)));

extern void switch_to(struct task *prev, struct task *next);
extern void switch_to_user(struct task *prev, struct task *next);
extern void fork_trampoline(void);
extern char stack_top;

volatile uint64_t counter_a = 0;
volatile uint64_t counter_b = 0;

extern void user_prog_entry(void);

static void create_kernel_task(int idx, void (*entry)(void), const char *name) {
	int n = 0;

	while (name[n] != '\0' && n < 15) {
		tasks[idx].name[n] = name[n];
		n++;
	}

	tasks[idx].name[n] = '\0';
	tasks[idx].pid = (uint32_t)idx;
	tasks[idx].ppid = INVALID_PID;
	tasks[idx].state = TASK_READY;
	tasks[idx].stack = task_stacks[idx];
	tasks[idx].exit_code = 0;
	tasks[idx].is_user = 0;
	tasks[idx].wake_tick = 0;
	tasks[idx].user_rip = 0;
	tasks[idx].user_rsp = 0;
	tasks[idx].user_rflags = 0;
	tasks[idx].user_stack_base = 0;

	tasks[idx].cr3 = vmm_create_task_pml4();

	if (tasks[idx].cr3 == 0) {
		tasks[idx].state = TASK_DEAD;
		return;
	}

	uint64_t *top = (uint64_t *)(task_stacks[idx] + TASK_STACK_SIZE);

	top -= 1;
	*top = (uint64_t)entry;
	top -= 1;
	*top = 0;
	top -= 1;
	*top = 0;
	top -= 1;
	*top = 0;
	top -= 1;
	*top = 0;
	top -= 1;
	*top = 0;
	top -= 1;
	*top = 0;

	tasks[idx].rsp = (uint64_t)top;
}

void task_init(void) {
	extern uint64_t pml4_table[];

	for (int i = 0; i < TASK_MAX; i++) {
		syscall_rsp[i] = (uint64_t)(uintptr_t)(task_stacks[i] + TASK_STACK_SIZE);
		tasks[i].pid = INVALID_PID;
		tasks[i].ppid = INVALID_PID;
		tasks[i].state = TASK_DEAD;
		tasks[i].rsp = 0;
		tasks[i].cr3 = 0;
		tasks[i].stack = 0;
		tasks[i].exit_code = 0;
		tasks[i].is_user = 0;
		tasks[i].wake_tick = 0;
		tasks[i].user_rip = 0;
		tasks[i].user_rsp = 0;
		tasks[i].user_rflags = 0;
		tasks[i].user_stack_base = 0;
		tasks[i].name[0] = '\0';
	}

	tasks[0].pid = 0;
	tasks[0].ppid = INVALID_PID;
	tasks[0].rsp = 0;
	tasks[0].cr3 = (uint64_t)(uintptr_t)pml4_table;
	tasks[0].state = TASK_RUNNING;
	tasks[0].stack = 0;
	tasks[0].exit_code = 0;
	tasks[0].is_user = 0;

	const char *shell_name = "shell";
	int n = 0;

	while (shell_name[n] != '\0' && n < 15) {
		tasks[0].name[n] = shell_name[n];
		n++;
	}

	tasks[0].name[n] = '\0';

	create_kernel_task(1, 0, "unused");
	tasks[1].state = TASK_DEAD;

	task_count = 2;
	current_task_idx = 0;
}

// 创建 ring3 用户任务，成功返回槽位号，失败返回 -1
int task_create_user(void (*entry)(void), const char *name) {
	int slot = -1;

	for (int i = 0; i < TASK_MAX; i++) {
		if (tasks[i].state == TASK_DEAD) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		return -1;
	}

	uint64_t cr3 = vmm_create_task_pml4();

	if (cr3 == 0) {
		return -1;
	}

	vmm_mark_user_page(cr3, (uint64_t)(uintptr_t)entry);
	vmm_mark_user_page(cr3, (uint64_t)(uintptr_t)user_stacks[slot]);
	vmm_mark_user_page(cr3, (uint64_t)(uintptr_t)(user_stacks[slot] + USER_STACK_SIZE - 1));

	int n = 0;

	while (name[n] != '\0' && n < 15) {
		tasks[slot].name[n] = name[n];
		n++;
	}

	tasks[slot].name[n] = '\0';
	tasks[slot].pid = (uint32_t)slot;
	tasks[slot].ppid = 0;
	tasks[slot].state = TASK_READY;
	tasks[slot].stack = task_stacks[slot];
	tasks[slot].cr3 = cr3;
	tasks[slot].exit_code = 0;
	tasks[slot].is_user = 1;
	tasks[slot].wake_tick = 0;
	tasks[slot].user_rip = (uint64_t)(uintptr_t)entry;
	tasks[slot].user_rsp = (uint64_t)(uintptr_t)(user_stacks[slot] + USER_STACK_SIZE);
	tasks[slot].user_rflags = 0x202;
	tasks[slot].user_stack_base = (uint64_t)(uintptr_t)user_stacks[slot];
	tasks[slot].rsp = 0;
	syscall_rsp[slot] = (uint64_t)(uintptr_t)(task_stacks[slot] + TASK_STACK_SIZE);

	if (slot >= task_count) {
		task_count = slot + 1;
	}

	return slot;
}

void schedule(void) {
	if (task_count < 2) {
		return;
	}

	int next = current_task_idx;

	for (int i = 1; i <= task_count; i++) {
		int idx = (current_task_idx + i) % task_count;

		if (tasks[idx].state == TASK_READY ||
		    tasks[idx].state == TASK_RUNNING) {
			next = idx;
			break;
		}
	}

	if (next == current_task_idx) {
		return;
	}

	struct task *prev = &tasks[current_task_idx];
	struct task *next_task = &tasks[next];

	int was_user = tasks[next].is_user;
	int first_run = (tasks[next].rsp == 0);

	current_task_idx = next;

	tss_set_rsp0(syscall_rsp[next]);

	if (was_user && first_run) {
		switch_to_user(prev, next_task);
	} else {
		switch_to(prev, next_task);
	}
}

void task_yield(void) {
	schedule();
}

int task_getpid(void) {
	return (int)tasks[current_task_idx].pid;
}

// 内核态 fork：复制父任务内核栈
static int task_fork_kernel(uint64_t *parent_rsp_ptr) {
	int slot = -1;

	for (int i = 0; i < TASK_MAX; i++) {
		if (tasks[i].state == TASK_DEAD) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		return -1;
	}

	uint64_t cr3 = vmm_create_task_pml4();

	if (cr3 == 0) {
		return -1;
	}

	uint64_t parent_rsp = (uint64_t)parent_rsp_ptr;
	uint64_t parent_stack_top;

	if (current_task_idx == 0) {
		parent_stack_top = (uint64_t)(uintptr_t)&stack_top;
	} else {
		parent_stack_top = (uint64_t)(task_stacks[current_task_idx] + TASK_STACK_SIZE);
	}

	uint64_t parent_stack_size = parent_stack_top - parent_rsp;
	uint64_t child_stack_top = (uint64_t)(task_stacks[slot] + TASK_STACK_SIZE);
	uint64_t child_rsp = child_stack_top - parent_stack_size - 8;

	if (child_rsp < (uint64_t)(task_stacks[slot])) {
		pmm_free_frame(cr3);
		return -1;
	}

	uint64_t parent_rbp = parent_rsp_ptr[5];
	uint64_t rbp_offset = parent_rbp - parent_rsp;
	uint64_t child_rbp = child_rsp + rbp_offset;

	uint64_t *child_slot = (uint64_t *)child_rsp;

	for (int i = 0; i < 5; i++) {
		child_slot[i] = parent_rsp_ptr[i];
	}

	child_slot[5] = child_rbp;
	child_slot[6] = (uint64_t)fork_trampoline;

	uint64_t *src = (uint64_t *)(parent_rsp + 48);
	uint64_t *dst = (uint64_t *)(child_rsp + 56);
	uint64_t remaining = parent_stack_size - 48;

	for (uint64_t i = 0; i < remaining / 8; i++) {
		dst[i] = src[i];
	}

	static int next_pid = 3;
	int pid = next_pid++;

	if (next_pid >= 1000) {
		next_pid = 3;
	}

	tasks[slot].pid = (uint32_t)pid;
	tasks[slot].ppid = tasks[current_task_idx].pid;
	tasks[slot].state = TASK_READY;
	tasks[slot].stack = task_stacks[slot];
	tasks[slot].cr3 = cr3;
	tasks[slot].exit_code = 0;
	tasks[slot].is_user = 0;
	tasks[slot].wake_tick = 0;

	const char *child_name = "child";
	int n = 0;

	while (child_name[n] != '\0' && n < 15) {
		tasks[slot].name[n] = child_name[n];
		n++;
	}

	tasks[slot].name[n] = '\0';
	tasks[slot].rsp = child_rsp;

	if (slot >= task_count) {
		task_count = slot + 1;
	}

	return pid;
}

// 用户态 fork：复制父任务用户栈和用户上下文
static int task_fork_user(void) {
	int slot = -1;

	for (int i = 0; i < TASK_MAX; i++) {
		if (tasks[i].state == TASK_DEAD) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		return -1;
	}

	uint64_t cr3 = vmm_create_task_pml4();

	if (cr3 == 0) {
		return -1;
	}

	extern uint64_t user_ctx_rip[TASK_MAX];
	extern uint64_t user_ctx_rsp[TASK_MAX];
	extern uint64_t user_ctx_rflags[TASK_MAX];
	extern uint64_t user_rsp_arr[TASK_MAX];

	vmm_mark_user_page(cr3, user_ctx_rip[current_task_idx]);
	vmm_mark_user_page(cr3, (uint64_t)(uintptr_t)user_stacks[slot]);
	vmm_mark_user_page(cr3, (uint64_t)(uintptr_t)(user_stacks[slot] + USER_STACK_SIZE - 1));

	uint64_t parent_stack_base = tasks[current_task_idx].user_stack_base;
	uint64_t parent_rsp = user_ctx_rsp[current_task_idx];
	uint64_t parent_offset = parent_rsp - parent_stack_base;
	uint64_t child_rsp = (uint64_t)(uintptr_t)(user_stacks[slot] + parent_offset);

	uint8_t *src = (uint8_t *)(uintptr_t)parent_stack_base;
	uint8_t *dst = user_stacks[slot];

	for (uint64_t i = 0; i < USER_STACK_SIZE; i++) {
		dst[i] = src[i];
	}

	static int next_pid = 3;
	int pid = next_pid++;

	if (next_pid >= 1000) {
		next_pid = 3;
	}

	int n = 0;
	const char *child_name = "user_child";

	while (child_name[n] != '\0' && n < 15) {
		tasks[slot].name[n] = child_name[n];
		n++;
	}

	tasks[slot].name[n] = '\0';
	tasks[slot].pid = (uint32_t)pid;
	tasks[slot].ppid = tasks[current_task_idx].pid;
	tasks[slot].state = TASK_READY;
	tasks[slot].stack = task_stacks[slot];
	tasks[slot].cr3 = cr3;
	tasks[slot].exit_code = 0;
	tasks[slot].is_user = 1;
	tasks[slot].wake_tick = 0;
	tasks[slot].user_rip = user_ctx_rip[current_task_idx];
	tasks[slot].user_rsp = child_rsp;
	tasks[slot].user_rflags = user_ctx_rflags[current_task_idx];
	tasks[slot].user_stack_base = (uint64_t)(uintptr_t)user_stacks[slot];
	tasks[slot].rsp = 0;
	syscall_rsp[slot] = (uint64_t)(uintptr_t)(task_stacks[slot] + TASK_STACK_SIZE);

	if (slot >= task_count) {
		task_count = slot + 1;
	}

	return pid;
}

int task_fork(void) {
	if (tasks[current_task_idx].is_user) {
		return task_fork_user();
	}

	return task_fork_kernel((uint64_t *)0);
}

void task_exit(int code) {
	tasks[current_task_idx].exit_code = code;
	tasks[current_task_idx].state = TASK_ZOMBIE;

	uint32_t ppid = tasks[current_task_idx].ppid;

	for (int i = 0; i < task_count; i++) {
		if (tasks[i].pid == ppid && tasks[i].state == TASK_BLOCKED) {
			tasks[i].state = TASK_READY;
			break;
		}
	}

	schedule();

	for (;;) {
		__asm__ volatile ("hlt");
	}
}

int task_wait(int *code) {
	uint32_t mypid = tasks[current_task_idx].pid;

	for (;;) {
		int pending = 0;

		for (int i = 0; i < task_count; i++) {
			if (tasks[i].ppid != mypid) {
				continue;
			}

			if (tasks[i].state == TASK_DEAD) {
				continue;
			}

			pending = 1;

			if (tasks[i].state == TASK_ZOMBIE) {
				int child_pid = (int)tasks[i].pid;

				if (code != 0) {
					*code = tasks[i].exit_code;
				}

				pmm_free_frame(tasks[i].cr3);
				tasks[i].cr3 = 0;
				tasks[i].state = TASK_DEAD;

				return child_pid;
			}
		}

		if (!pending) {
			return -1;
		}

		tasks[current_task_idx].state = TASK_BLOCKED;
		schedule();
	}
}

void task_sleep(uint32_t ms) {
	tasks[current_task_idx].state = TASK_BLOCKED;
	tasks[current_task_idx].wake_tick = ticks + ms;
	schedule();
}

void task_wake_check(void) {
	for (int i = 0; i < task_count; i++) {
		if (tasks[i].state == TASK_BLOCKED && tasks[i].wake_tick != 0) {
			if (ticks >= tasks[i].wake_tick) {
				tasks[i].state = TASK_READY;
				tasks[i].wake_tick = 0;
			}
		}
	}
}