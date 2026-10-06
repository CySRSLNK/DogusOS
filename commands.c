#include <stdint.h>
#include "kernel.h"
#include "shell.h"
#include "commands.h"
#include "ext2.h"
#include "users.h"
#include "strings.h"

int check_permission(const char *source, int need, int cur);
int check_argc(const char *source, int min, int max, int cur);

struct cmd commands[] = {
	{"cat", cmd_cat, str_cmd_cat_summary, str_cmd_cat_detail},
	{"cd", cmd_cd, str_cmd_cd_summary, str_cmd_cd_detail},
	{"clear", cmd_clear, str_cmd_clear_summary, str_cmd_clear_detail},
	{"echo", cmd_echo, str_cmd_echo_summary, str_cmd_echo_detail},
	{"false", cmd_false, str_cmd_false_summary, str_cmd_false_detail},
	{"help", cmd_help, str_cmd_help_summary, str_cmd_help_detail},
	{"history", cmd_history, str_cmd_history_summary, str_cmd_history_detail},
	{"hostname", cmd_hostname, str_cmd_hostname_summary, str_cmd_hostname_detail},
	{"id", cmd_id, str_cmd_id_summary, str_cmd_id_detail},
	{"ls", cmd_ls, str_cmd_ls_summary, str_cmd_ls_detail},
	{"mkdir", cmd_mkdir, str_cmd_mkdir_summary, str_cmd_mkdir_detail},
	{"pwd", cmd_pwd, str_cmd_pwd_summary, str_cmd_pwd_detail},
	{"reboot", cmd_reboot, str_cmd_reboot_summary, str_cmd_reboot_detail},
	{"rm", cmd_rm, str_cmd_rm_summary, str_cmd_rm_detail},
	{"rmdir", cmd_rmdir, str_cmd_rmdir_summary, str_cmd_rmdir_detail},
	{"stat", cmd_stat, str_cmd_stat_summary, str_cmd_stat_detail},
	{"su", cmd_su, str_cmd_su_summary, str_cmd_su_detail},
	{"sudo", cmd_sudo, str_cmd_sudo_summary, str_cmd_sudo_detail},
	{"touch", cmd_touch, str_cmd_touch_summary, str_cmd_touch_detail},
	{"tree", cmd_tree, str_cmd_tree_summary, str_cmd_tree_detail},
	{"true", cmd_true, str_cmd_true_summary, str_cmd_true_detail},
	{"uptime", cmd_uptime, str_cmd_uptime_summary, str_cmd_uptime_detail},
	{"useradd", cmd_useradd, str_cmd_useradd_summary, str_cmd_useradd_detail},
	{"userdel", cmd_userdel, str_cmd_userdel_summary, str_cmd_userdel_detail},
	{"whoami", cmd_whoami, str_cmd_whoami_summary, str_cmd_whoami_detail},
	{"write", cmd_write, str_cmd_write_summary, str_cmd_write_detail},
};

int command_count = sizeof(commands) / sizeof(commands[0]);

static void build_abs_path(const char *arg, char *norm, int norm_size) {
	char joined[256];

	if (arg[0] == '/') {
		int w = 0;
		while (arg[w] != '\0' && w < 255) {
			joined[w] = arg[w];
			w++;
		}
		joined[w] = '\0';
	} else {
		int w = 0;

		while (cwd[w] != '\0' && w < 255) {
			joined[w] = cwd[w];
			w++;
		}

		if (w > 0 && joined[w - 1] != '/') {
			joined[w++] = '/';
		}

		for (int i = 0; arg[i] != '\0' && w < 255; i++) {
			joined[w++] = arg[i];
		}

		joined[w] = '\0';
	}

	path_normalize(joined, norm, norm_size);
}

int cmd_cat(int argc, char **argv, int privilege) {
	if (check_permission("cat", 0, privilege)) {
		if (check_argc("cat", 1, -1, argc)) {
			int ret = 0;

			for (int i = 0; i < argc; i++) {
				char norm[256];
				build_abs_path(argv[i], norm, 256);

				int r = ext2_cat(norm);

				if (r == -1) {
					print_error("cat", str_msg_noent, argv[i]);
					ret = 1;
				} else if (r == -2) {
					print_error("cat", str_msg_isdir, argv[i]);
					ret = 1;
				}
			}

			return ret;
		}
	}

	return 1;
}

int cmd_cd(int argc, char **argv, int privilege) {
	if (check_permission("cd", 0, privilege)) {
		if (check_argc("cd", 0, 1, argc)) {
			const char *display;
			char norm[256];

			if (argc == 0) {
				display = "/";
				norm[0] = '/';
				norm[1] = '\0';
			} else {
				display = argv[0];
				build_abs_path(argv[0], norm, 256);
			}

			uint32_t ino;

			if (ext2_lookup(norm, &ino) != 0) {
				print_error("cd", str_msg_noent, display);
				return 1;
			}

			struct ext2_inode inode;
			ext2_read_inode(ino, &inode);

			if ((inode.i_mode & 0xF000) != 0x4000) {
				print_error("cd", str_msg_notdir, display);
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

	if (check_permission("clear", 0, privilege)) {
		if (check_argc("clear", 0, 0, argc)) {
			clear_screen();
			cursor_goto(0, 0);
			return 0;
		}
	}

	return 1;
}

int cmd_echo(int argc, char **argv, int privilege) {
	if (check_permission("echo", 0, privilege)) {
		if (check_argc("echo", 0, -1, argc)) {
			int newline = 1;
			int start = 0;

			if (argc > 0 && argv[0][0] == '-' && argv[0][1] != '\0') {
				int all_n = 1;

				for (int j = 1; argv[0][j] != '\0'; j++) {
					if (argv[0][j] != 'n' && argv[0][j] != 'N') {
						all_n = 0;
						break;
					}
				}

				if (all_n) {
					newline = 0;
					start = 1;
				}
			}

			for (int i = start; i < argc; i++) {
				if (i > start) {
					print_char(' ');
				}

				print(argv[i]);
			}

			if (newline) {
				print_char('\n');
			}

			return 0;
		}
	}

	return 1;
}

int cmd_false(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission("false", 0, privilege)) {
		if (check_argc("false", 0, 0, argc)) {
			return 1;
		}
	}

	return 1;
}

int cmd_help(int argc, char **argv, int privilege) {
	if (check_permission("help", 0, privilege)) {
		if (check_argc("help", 0, 1, argc)) {
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

	if (check_permission("history", 0, privilege)) {
		if (check_argc("history", 0, 0, argc)) {
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

	if (check_permission("hostname", 0, privilege)) {
		if (check_argc("hostname", 0, 0, argc)) {
			print(host);
			print_char('\n');
			return 0;
		}
	}

	return 1;
}

int cmd_id(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission("id", 0, privilege)) {
		if (check_argc("id", 0, 0, argc)) {
			uint32_t gid = 0;

			users_gid_by_uid(current_uid, &gid);

			print("uid=");
			print_uint(current_uid);

			print("(");
			print(current_user);
			print(")");

			print(" gid=");
			print_uint(gid);

			print_char('\n');
			return 0;
		}
	}

	return 1;
}

int cmd_ls(int argc, char **argv, int privilege) {
	if (check_permission("ls", 0, privilege)) {
		if (check_argc("ls", 0, 4, argc)) {
			int show_hidden = 0;
			int show_inode = 0;
			int show_long = 0;
			const char *path = 0;
			int path_count = 0;

			for (int i = 0; i < argc; i++) {
				if (argv[i][0] == '-' && argv[i][1] != '\0') {
					for (int j = 1; argv[i][j] != '\0'; j++) {
						if (argv[i][j] == 'a' || argv[i][j] == 'A') {
							show_hidden = 1;
						} else if (argv[i][j] == 'i' || argv[i][j] == 'I') {
							show_inode = 1;
						} else if (argv[i][j] == 'l' || argv[i][j] == 'L') {
							show_long = 1;
						} else {
							print_error("ls", str_msg_invalid_option, argv[i]);
							return 1;
						}
					}

					continue;
				}

				path = argv[i];
				path_count++;
			}

			if (path_count > 1) {
				print_error("ls", str_msg_too_many, 0);
				return 1;
			}

			char norm[256];
			const char *display;

			if (path == 0) {
				display = ".";
				for (int i = 0; i < 256; i++) {
					norm[i] = cwd[i];

					if (cwd[i] == '\0') {
						break;
					}
				}
			} else {
				display = path;
				build_abs_path(path, norm, 256);
			}

			uint32_t ino;

			if (ext2_lookup(norm, &ino) != 0) {
				print_error("ls", str_msg_noent, display);
				return 1;
			}

			struct ext2_inode inode;
			ext2_read_inode(ino, &inode);

			if ((inode.i_mode & 0xF000) != 0x4000) {
				print_error("ls", str_msg_notdir, display);
				return 1;
			}

			ext2_ls(norm, show_hidden, show_inode, show_long);
			return 0;
		}
	}

	return 1;
}

int cmd_mkdir(int argc, char **argv, int privilege) {
	if (check_permission("mkdir", 0, privilege)) {
		if (check_argc("mkdir", 1, -1, argc)) {
			int ret = 0;

			for (int i = 0; i < argc; i++) {
				char norm[256];
				build_abs_path(argv[i], norm, 256);

				int r = ext2_mkdir(norm);

				if (r == 0) {
					continue;
				}

				if (r == -1) {
					print_error("mkdir", str_msg_noent, argv[i]);
				} else if (r == -2) {
					print_error("mkdir", str_msg_exists, argv[i]);
				} else if (r == -3) {
					print_error("mkdir", str_msg_notdir, argv[i]);
				} else {
					print_error("mkdir", str_msg_nospace, argv[i]);
				}

				ret = 1;
			}

			return ret;
		}
	}

	return 1;
}

int cmd_pwd(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission("pwd", 0, privilege)) {
		if (check_argc("pwd", 0, 0, argc)) {
			print(cwd);
			print_char('\n');
			return 0;
		}
	}

	return 1;
}

int cmd_reboot(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission("reboot", 0, privilege)) {
		if (check_argc("reboot", 0, 0, argc)) {
			__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0xFE), "d"((uint16_t)0x64));

			while (1) {
				__asm__ volatile ("hlt");
			}
		}
	}

	return 1;
}

int cmd_rm(int argc, char **argv, int privilege) {
	if (check_permission("rm", 1, privilege)) {
		if (check_argc("rm", 1, -1, argc)) {
			int recursive = 0;
			int force = 0;
			int ret = 0;

			for (int i = 0; i < argc; i++) {
				if (argv[i][0] == '-' && argv[i][1] != '\0') {
					for (int j = 1; argv[i][j] != '\0'; j++) {
						if (argv[i][j] == 'r' || argv[i][j] == 'R') {
							recursive = 1;
						} else if (argv[i][j] == 'f' || argv[i][j] == 'F') {
							force = 1;
						} else {
							print_error("rm", str_msg_invalid_option, argv[i]);
							return 1;
						}
					}

					continue;
				}

				char norm[256];
				build_abs_path(argv[i], norm, 256);

				int r = ext2_remove(norm, recursive);

				if (r == 0) {
					continue;
				}

				if (r == -2 && force) {
					continue;
				}

				if (r == -1 || r == -2) {
					print_error("rm", str_msg_noent, argv[i]);
				} else if (r == -3) {
					print_error("rm", str_msg_notdir, argv[i]);
				} else if (r == -4) {
					print_error("rm", str_msg_isdir, argv[i]);
				}

				ret = 1;
			}

			return ret;
		}
	}

	return 1;
}

int cmd_rmdir(int argc, char **argv, int privilege) {
	if (check_permission("rmdir", 1, privilege)) {
		if (check_argc("rmdir", 1, -1, argc)) {
			int ret = 0;

			for (int i = 0; i < argc; i++) {
				char norm[256];
				build_abs_path(argv[i], norm, 256);

				int r = ext2_rmdir(norm);

				if (r == 0) {
					continue;
				}

				if (r == -1 || r == -2) {
					print_error("rmdir", str_msg_noent, argv[i]);
				} else if (r == -3 || r == -4) {
					print_error("rmdir", str_msg_notdir, argv[i]);
				} else {
					print_error("rmdir", str_msg_notempty, argv[i]);
				}

				ret = 1;
			}

			return ret;
		}
	}

	return 1;
}

int cmd_stat(int argc, char **argv, int privilege) {
	if (check_permission("stat", 0, privilege)) {
		if (check_argc("stat", 1, 1, argc)) {
			char norm[256];
			build_abs_path(argv[0], norm, 256);

			uint32_t ino;

			if (ext2_lookup(norm, &ino) != 0) {
				print_error("stat", str_msg_noent, argv[0]);
				return 1;
			}

			struct ext2_inode inode;
			ext2_read_inode(ino, &inode);

			uint16_t mode = inode.i_mode & 0xF000;
			const char *type;

			if (mode == 0x8000) {
				type = "regular file";
			} else if (mode == 0x4000) {
				type = "directory";
			} else if (mode == 0xA000) {
				type = "symbolic link";
			} else if (mode == 0x2000) {
				type = "character device";
			} else if (mode == 0x6000) {
				type = "block device";
			} else if (mode == 0x1000) {
				type = "fifo";
			} else if (mode == 0xC000) {
				type = "socket";
			} else {
				type = "unknown";
			}

			print("  File: ");
			print(argv[0]);
			print_char('\n');

			print("  Size: ");
			print_uint(inode.i_size);
			print("          Blocks: ");
			print_uint(inode.i_blocks);
			print("          Type: ");
			print(type);
			print_char('\n');

			print("  Inode: ");
			print_uint(ino);
			print("          Links: ");
			print_uint(inode.i_links_count);
			print_char('\n');

			return 0;
		}
	}

	return 1;
}

int cmd_su(int argc, char **argv, int privilege) {
	(void)privilege;

	if (check_argc("su", 0, 1, argc)) {
		const char *target_name;

		if (argc == 0) {
			target_name = "root";
		} else {
			target_name = argv[0];
		}

		uint32_t uid;

		if (users_uid_by_name(target_name, &uid) != 0) {
			print_error("su", "user not found", target_name);
			return 1;
		}

		current_uid = uid;
		current_euid = uid;

		if (users_name_by_uid(uid, current_user, 32) != 0) {
			print_error("su", "user not found", target_name);
			return 1;
		}

		build_prompt();
		return 0;
	}

	return 1;
}

int cmd_sudo(int argc, char **argv, int privilege) {
	(void)privilege;

	if (argc < 1) {
		return 0;
	}

	for (int i = 0; i < command_count; i++) {
		if (strcmp(argv[0], commands[i].name) == 0) {
			uint32_t saved_euid = current_euid;
			current_euid = 0;

			int ret = commands[i].func(argc - 1, argv + 1, 1);

			current_euid = saved_euid;
			return ret;
		}
	}

	print_error("sudo", str_msg_cmd_not_found, argv[0]);
	return 1;
}

int cmd_touch(int argc, char **argv, int privilege) {
	if (check_permission("touch", 0, privilege)) {
		if (check_argc("touch", 1, -1, argc)) {
			int ret = 0;

			for (int i = 0; i < argc; i++) {
				char norm[256];
				build_abs_path(argv[i], norm, 256);

				uint32_t existing;
				if (ext2_lookup(norm, &existing) == 0) {
					continue;
				}

				int r = ext2_write_file(norm, "", 0, 0);

				if (r == -1) {
					print_error("touch", str_msg_noent, argv[i]);
					ret = 1;
				} else if (r == -2) {
					print_error("touch", str_msg_noent, argv[i]);
					ret = 1;
				} else if (r == -3) {
					print_error("touch", str_msg_notdir, argv[i]);
					ret = 1;
				} else if (r == -4) {
					print_error("touch", str_msg_isdir, argv[i]);
					ret = 1;
				} else if (r == -5) {
					print_error("touch", str_msg_nospace, argv[i]);
					ret = 1;
				}
			}

			return ret;
		}
	}

	return 1;
}

int cmd_tree(int argc, char **argv, int privilege) {
	if (check_permission("tree", 0, privilege)) {
		if (check_argc("tree", 0, 1, argc)) {
			char norm[256];
			const char *display;

			if (argc == 0) {
				display = ".";
				for (int i = 0; i < 256; i++) {
					norm[i] = cwd[i];

					if (cwd[i] == '\0') {
						break;
					}
				}
			} else {
				display = argv[0];
				build_abs_path(argv[0], norm, 256);
			}

			uint32_t ino;

			if (ext2_lookup(norm, &ino) != 0) {
				print_error("tree", str_msg_noent, display);
				return 1;
			}

			struct ext2_inode inode;
			ext2_read_inode(ino, &inode);

			if ((inode.i_mode & 0xF000) != 0x4000) {
				print_error("tree", str_msg_notdir, display);
				return 1;
			}

			print(display);
			print_char('\n');
			ext2_tree(norm, 0);
			return 0;
		}
	}

	return 1;
}

int cmd_true(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission("true", 0, privilege)) {
		if (check_argc("true", 0, 0, argc)) {
			return 0;
		}
	}

	return 1;
}

int cmd_uptime(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission("uptime", 0, privilege)) {
		if (check_argc("uptime", 0, 0, argc)) {
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

int cmd_useradd(int argc, char **argv, int privilege) {
	if (check_permission("useradd", 1, privilege)) {
		if (check_argc("useradd", 1, 1, argc)) {
			int r = users_add(argv[0]);

			if (r == 0) {
				return 0;
			}

			if (r == -1) {
				print_error("useradd", "user already exists", argv[0]);
			} else if (r == -2) {
				print_error("useradd", "too many users", 0);
			} else {
				print_error("useradd", "invalid name", argv[0]);
			}

			return 1;
		}
	}

	return 1;
}

int cmd_userdel(int argc, char **argv, int privilege) {
	if (check_permission("userdel", 1, privilege)) {
		if (check_argc("userdel", 1, 1, argc)) {
			int is_current = 1;
			int j = 0;

			while (current_user[j] != '\0' || argv[0][j] != '\0') {
				if (current_user[j] != argv[0][j]) {
					is_current = 0;
					break;
				}

				j++;
			}

			if (is_current) {
				print_error("userdel", "cannot remove current user", argv[0]);
				return 1;
			}

			int r = users_del(argv[0]);

			if (r == 0) {
				return 0;
			}

			print_error("userdel", "user not found", argv[0]);
			return 1;
		}
	}

	return 1;
}

int cmd_whoami(int argc, char **argv, int privilege) {
	(void)argv;

	if (check_permission("whoami", 0, privilege)) {
		if (check_argc("whoami", 0, 0, argc)) {
			print(current_user);
			print_char('\n');
			return 0;
		}
	}

	return 1;
}

int cmd_write(int argc, char **argv, int privilege) {
	if (check_permission("write", 0, privilege)) {
		if (check_argc("write", 1, -1, argc)) {
			int append = 0;
			int newline = 1;
			int start = 0;

			for (int i = 0; i < argc; i++) {
				if (argv[i][0] == '-' && argv[i][1] != '\0') {
					int all_valid = 1;

					for (int j = 1; argv[i][j] != '\0'; j++) {
						if (argv[i][j] == 'a' || argv[i][j] == 'A') {
							append = 1;
						} else if (argv[i][j] == 'n' || argv[i][j] == 'N') {
							newline = 0;
						} else {
							all_valid = 0;
							break;
						}
					}

					if (all_valid) {
						start = i + 1;
						continue;
					}

					print_error("write", str_msg_invalid_option, argv[i]);
					return 1;
				}

				start = i;
				break;
			}

			if (start >= argc) {
				print_error("write", str_msg_too_few, 0);
				return 1;
			}

			const char *file = argv[start];
			char norm[256];
			build_abs_path(file, norm, 256);

			static char buf[49152];
			int w = 0;

			for (int i = start + 1; i < argc; i++) {
				if (i > start + 1) {
					if (w < 49150) {
						buf[w++] = ' ';
					}
				}

				for (int j = 0; argv[i][j] != '\0' && w < 49150; j++) {
					buf[w++] = argv[i][j];
				}
			}

			if (newline) {
				buf[w++] = '\n';
			}

			int r = ext2_write_file(norm, buf, (uint32_t)w, append);

			if (r == 0) {
				return 0;
			}

			if (r == -1) {
				print_error("write", str_msg_noent, file);
			} else if (r == -2) {
				print_error("write", str_msg_noent, file);
			} else if (r == -3) {
				print_error("write", str_msg_notdir, file);
			} else if (r == -4) {
				print_error("write", str_msg_isdir, file);
			} else {
				print_error("write", str_msg_nospace, file);
			}

			return 1;
		}
	}

	return 1;
}