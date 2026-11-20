// ring3 用户程序，打印、fork、忙等
#include <stdint.h>
#include "user_prog.h"

static long sys_call3(long num, long a1, long a2, long a3) {
	long ret;

	__asm__ volatile (
		"syscall"
		: "=a"(ret)
		: "a"(num), "D"(a1), "S"(a2), "d"(a3)
		: "rcx", "r11", "memory"
	);

	return ret;
}

static long sys_call0(long num) {
	return sys_call3(num, 0, 0, 0);
}

static int slen(const char *s) {
	int n = 0;
	while (s[n] != '\0') {
		n++;
	}
	return n;
}

static void uprint(const char *s) {
	sys_call3(1, 1, (long)s, slen(s));
}

static void uprint_pid_tag(char tag) {
	char buf[8];
	int pid = (int)sys_call0(39);

	buf[0] = '[';
	buf[1] = tag;
	buf[2] = ' ';
	buf[3] = '0' + (pid % 10);
	buf[4] = ']';
	buf[5] = ' ';
	buf[6] = '\0';
	uprint(buf);
}

__attribute__((section(".user_test")))
void user_prog_entry(void) {
	uprint_pid_tag('A');
	uprint("start\n");

	long child = sys_call0(57);

	if (child == 0) {
		for (;;) {
			uprint_pid_tag('B');
			uprint("tick\n");
			for (volatile int i = 0; i < 3000000; i++) {}
		}
	}

	for (;;) {
		uprint_pid_tag('A');
		uprint("tick\n");
		for (volatile int i = 0; i < 3000000; i++) {}
	}
}