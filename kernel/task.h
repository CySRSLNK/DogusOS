#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX	8
#define TASK_STACK_SIZE	16384
#define USER_STACK_SIZE	8192

#define TASK_RUNNING	0
#define TASK_READY	1
#define TASK_BLOCKED	2
#define TASK_ZOMBIE	3
#define TASK_DEAD	4

struct task {
	uint64_t rsp;
	uint64_t cr3;
	uint8_t *stack;
	uint64_t user_rip;
	uint64_t user_rsp;
	uint64_t user_rflags;
	uint64_t user_stack_base;
	uint64_t wake_tick;
	uint32_t pid;
	uint32_t ppid;
	int state;
	int exit_code;
	int is_user;
	char name[16];
};

extern struct task tasks[TASK_MAX];
extern int task_count;
extern int current_task_idx;
extern uint64_t syscall_rsp[TASK_MAX];
extern uint64_t user_rsp_arr[TASK_MAX];
extern uint64_t user_ctx_rip[TASK_MAX];
extern uint64_t user_ctx_rsp[TASK_MAX];
extern uint64_t user_ctx_rflags[TASK_MAX];

void task_init(void);
void schedule(void);
void task_yield(void);

int task_fork(void);
void task_exit(int code);
int task_wait(int *code);
int task_getpid(void);
void task_sleep(uint32_t ms);
void task_wake_check(void);
int task_create_user(void (*entry)(void), const char *name);

#endif