#include <stdint.h>
#include "kernel.h"
#include "shell.h"
#include "commands.h"
#include "strings.h"

int check_permission(int need, int cur);
int check_argc(int min, int max, int cur);

struct cmd commands[] = {
	{"clear", cmd_clear, str_cmd_clear_summary, str_cmd_clear_detail},
	{"echo", cmd_echo, str_cmd_echo_summary, str_cmd_echo_detail},
	{"false", cmd_false, str_cmd_false_summary, str_cmd_false_detail},
	{"help", cmd_help, str_cmd_help_summary, str_cmd_help_detail},
	{"history", cmd_history, str_cmd_history_summary, str_cmd_history_detail},
	{"hostname", cmd_hostname, str_cmd_hostname_summary, str_cmd_hostname_detail},
	{"reboot", cmd_reboot, str_cmd_reboot_summary, str_cmd_reboot_detail},
	{"true", cmd_true, str_cmd_true_summary, str_cmd_true_detail},
	{"uptime", cmd_uptime, str_cmd_uptime_summary, str_cmd_uptime_detail},
	{"whoami", cmd_whoami, str_cmd_whoami_summary, str_cmd_whoami_detail},
};

int command_count = sizeof(commands) / sizeof(commands[0]);

int cmd_clear(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			clear_screen();
			cursor_goto(0, 0);
			return 0;
		}
	}

	return 1;
}

int cmd_echo(int argc, char **argv, int privilege) {
	if (check_permission(0, privilege)) {
		if (check_argc(0, -1, argc)) {
			for (int i = 0; i < argc; i++) {
				if (i > 0) {
					print_char(' ');
				}

				print(argv[i]);
			}

			print_char('\n');
			return 0;
		}
	}

	return 1;
}

int cmd_false(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			return 1;
		}
	}

	return 1;
}

int cmd_help(int argc, char **argv, int privilege) {
	if (check_permission(0, privilege)) {
		if (check_argc(0, 1, argc)) {
			if (argc == 0) {
				for (int i = 0; i < command_count; i++) {
					print(commands[i].name);
					print(" - ");
					print(commands[i].summary);
					print_char('\n');
				}
			} else {
				for (int i = 0; i < command_count; i++) {
					if (strcmp(argv[0], commands[i].name) == 0) {
						print(commands[i].name);
						print(" - ");
						print(commands[i].detail);
						print_char('\n');
						return 0;
					}
				}

				print_cmd_not_found(str_src_help, argv[0]);
				return 1;
			}

			return 0;
		}
	}

	return 1;
}

int cmd_history(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			for (int i = 0; i < hist_count; i++) {
				print(" ");
				print_uint((uint32_t)(i + 1));
				print(" ");
				print(hist[i]);
				print_char('\n');
			}

			return 0;
		}
	}

	return 1;
}

int cmd_hostname(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			print(host);
			print_char('\n');
			return 0;
		}
	}

	return 1;
}

int cmd_reboot(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0xFE), "d"((uint16_t)0x64));

			while (1) {
				__asm__ volatile ("hlt");
			}
		}
	}

	return 1;
}

int cmd_true(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			return 0;
		}
	}

	return 1;
}

int cmd_uptime(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			uint64_t sec = ticks / 1000;
			uint64_t min = sec / 60;
			uint64_t hour = min / 60;
			uint64_t day = hour / 24;

			uint32_t m = (uint32_t)(min % 60);
			uint32_t s = (uint32_t)(sec % 60);

			print(str_uptime_up);
			print_uint((uint32_t)day);
			print(str_uptime_days);
			print_uint((uint32_t)(hour % 24));
			print_char(':');

			if (m < 10) {
				print_char('0');
			}
			print_uint(m);
			print_char(':');

			if (s < 10) {
				print_char('0');
			}
			print_uint(s);
			print_char('\n');
			return 0;
		}
	}

	return 1;
}

int cmd_whoami(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission(0, privilege)) {
		if (check_argc(0, 0, argc)) {
			print(user);
			print_char('\n');
			return 0;
		}
	}

	return 1;
}