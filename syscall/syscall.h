#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE	1
#define SYS_SLEEP	35
#define SYS_GETPID	39
#define SYS_FORK	57
#define SYS_EXIT	60
#define SYS_WAIT	61

void syscall_init(void);

#endif