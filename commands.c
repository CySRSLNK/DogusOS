#include <stdint.h>
#include "kernel.h"
#include "shell.h"
#include "commands.h"
#include "ext2.h"
#include "strings.h"

int check_permission(int need, int cur);
int check_argc(int min, int max, int cur);

struct cmd commands[] = {
	{"cd", cmd_cd, str_cmd_cd_summary, str_cmd_cd_detail},
	{"clear", cmd_clear, str_cmd_clear_summary, str_cmd_clear_detail},
	{"echo", cmd_echo, str_cmd_echo_summary, str_cmd_echo_detail},
	{"false", cmd_false, str_cmd_false_summary, str_cmd_false_detail},
	{"help", cmd_help, str_cmd_help_summary, str_cmd_help_detail},
	{"history", cmd_history, str_cmd_history_summary, str_cmd_history_detail},
	{"hostname", cmd_hostname, str_cmd_hostname_summary, str_cmd_hostname_detail},
	{"ls", cmd_ls, str_cmd_ls_summary, str_cmd_ls_detail},
	{"reboot", cmd_reboot, str_cmd_reboot_summary, str_cmd_reboot_detail},
	{"true", cmd_true, str_cmd_true_summary, str_cmd_true_detail},
	{"uptime", cmd_uptime, str_cmd_uptime_summary, str_cmd_uptime_detail},
	{"whoami", cmd_whoami, str_cmd_whoami_summary, str_cmd_whoami_detail},
};

int command_count = sizeof(commands) / sizeof(commands[0]);

int cmd_cd(int argc, char **argv, int privilege) {
	if (check_permission(0, privilege)) {
		if (check_argc(0, 1, argc)) {
			const char *target;
			char joined[256];
			char norm[256];

			if (argc == 0) {
				target = "/";
			} else if (argv[0][0] == '/') {
				target = argv[0];
			} else {
				int w = 0;

				for (int i = 0; cwd[i] != '\0' && w < 255; i++) {
					joined[w++] = cwd[i];
				}

				if (w > 0 && joined[w - 1] != '/') {
					joined[w++] = '/';
				}

				for (int i = 0; argv[0][i] != '\0' && w < 255; i++) {
					joined[w++] = argv[0][i];
				}

				joined[w] = '\0';
				target = joined;
			}

			path_normalize(target, norm, 256);

			uint32_t ino;

			if (ext2_lookup(norm, &ino) != 0) {
				print(str_cd_noent);
				print(norm);
				print_char('\n');
				return 1;
			}

			struct ext2_inode inode;
			ext2_read_inode(ino, &inode);

			if ((inode.i_mode & 0xF000) != 0x4000) {
				print(str_cd_notdir);
				print(norm);
				print_char('\n');
				return 1;
			}

			for (int i = 0; i < 256; i++) {
				cwd[i] = norm[i];
				path_buf[i] = norm[i];

				if (norm[i] == '\0') {
					break;
				}
			}

			build_prompt();
			return 0;
		}
	}

	return 1;
}

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

				print_cmd_not_found("help", argv[0]);
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

int cmd_ls(int argc, char **argv, int privilege) {
	if (check_permission(0, privilege)) {
		if (check_argc(0, 1, argc)) {
			char joined[256];
			char norm[256];
			const char *target;
			const char *display;

			if (argc == 0) {
				target = cwd;
				display = ".";
			} else if (argv[0][0] == '/') {
				target = argv[0];
				display = argv[0];
			} else {
				int w = 0;

				for (int i = 0; cwd[i] != '\0' && w < 255; i++) {
					joined[w++] = cwd[i];
				}

				if (w > 0 && joined[w - 1] != '/') {
					joined[w++] = '/';
				}

				for (int i = 0; argv[0][i] != '\0' && w < 255; i++) {
					joined[w++] = argv[0][i];
				}

				joined[w] = '\0';
				target = joined;
				display = argv[0];
			}

			path_normalize(target, norm, 256);

			uint32_t ino;

			if (ext2_lookup(norm, &ino) != 0) {
				print(str_ls_noent);
				print(display);
				print_char('\n');
				return 1;
			}

			struct ext2_inode inode;
			ext2_read_inode(ino, &inode);

			if ((inode.i_mode & 0xF000) != 0x4000) {
				print(str_ls_notdir);
				print(display);
				print_char('\n');
				return 1;
			}

			ext2_ls(norm);
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