// DogusOS 命令表与命令实现
#include <stdint.h>
#include "kernel.h"
#include "shell.h"
#include "commands.h"
#include "ext2.h"
#include "users.h"
#include "strings.h"
#include "version.h"

int check_argc(const char *source, int min, int max, int cur);

struct cmd commands[] = {
	{"cat", cmd_cat, str_cmd_cat_summary, str_cmd_cat_detail},
	{"cd", cmd_cd, str_cmd_cd_summary, str_cmd_cd_detail},
	{"clear", cmd_clear, str_cmd_clear_summary, str_cmd_clear_detail},
	{"count", cmd_count, str_cmd_count_summary, str_cmd_count_detail},
	{"echo", cmd_echo, str_cmd_echo_summary, str_cmd_echo_detail},
	{"false", cmd_false, str_cmd_false_summary, str_cmd_false_detail},
	{"help", cmd_help, str_cmd_help_summary, str_cmd_help_detail},
	{"history", cmd_history, str_cmd_history_summary, str_cmd_history_detail},
	{"hostname", cmd_hostname, str_cmd_hostname_summary, str_cmd_hostname_detail},
	{"id", cmd_id, str_cmd_id_summary, str_cmd_id_detail},
	{"ls", cmd_ls, str_cmd_ls_summary, str_cmd_ls_detail},
	{"mkdir", cmd_mkdir, str_cmd_mkdir_summary, str_cmd_mkdir_detail},
	{"passwd", cmd_passwd, str_cmd_passwd_summary, str_cmd_passwd_detail},
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
	{"uname", cmd_uname, str_cmd_uname_summary, str_cmd_uname_detail},
	{"uptime", cmd_uptime, str_cmd_uptime_summary, str_cmd_uptime_detail},
	{"useradd", cmd_useradd, str_cmd_useradd_summary, str_cmd_useradd_detail},
	{"userdel", cmd_userdel, str_cmd_userdel_summary, str_cmd_userdel_detail},
	{"usermod", cmd_usermod, str_cmd_usermod_summary, str_cmd_usermod_detail},
	{"users", cmd_users, str_cmd_users_summary, str_cmd_users_detail},
	{"whoami", cmd_whoami, str_cmd_whoami_summary, str_cmd_whoami_detail},
	{"write", cmd_write, str_cmd_write_summary, str_cmd_write_detail},
};

int command_count = sizeof(commands) / sizeof(commands[0]);

// 把相对路径拼到 cwd 后，规范化为绝对路径
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

// 逐字符解析短选项
// valid 为合法字母集合，纯小写，opt_next 把大写统一转为小写
// 返回 0 取到一个选项字符，写入 ret
// 返回 1 选项结束，st->index 指向第一个非选项参数
// 返回 -1 遇到非法选项，已报错
int opt_next(const char *source, int argc, char **argv, const char *valid,
	     struct opt_state *st, char *ret) {
	while (st->index < argc) {
		const char *tok = argv[st->index];

		if (st->sub == 0) {
			if (tok[0] != '-') {
				return 1;
			}

			if (tok[1] == '\0') {
				return 1;
			}

			st->sub = 1;
		}

		char c = tok[st->sub];

		if (c == '\0') {
			st->index++;
			st->sub = 0;
			continue;
		}

		char lc = c;

		if (lc >= 'A' && lc <= 'Z') {
			lc = lc - 'A' + 'a';
		}

		int found = 0;

		for (int i = 0; valid[i] != '\0'; i++) {
			if (valid[i] == lc) {
				found = 1;
				break;
			}
		}

		if (!found) {
			print_error(source, str_msg_invalid_option, tok);
			return -1;
		}

		*ret = lc;
		st->sub++;
		return 0;
	}

	return 1;
}

int cmd_cat(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("cat", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("cat", 1, -1, argc - st.index)) {
		return 1;
	}

	int ret = 0;

	for (int i = st.index; i < argc; i++) {
		char norm[256];
		build_abs_path(argv[i], norm, 256);

		int rr = ext2_cat(norm);

		if (rr == -1) {
			print_error("cat", str_msg_noent, argv[i]);
			ret = 1;
		} else if (rr == -2) {
			print_error("cat", str_msg_isdir, argv[i]);
			ret = 1;
		} else if (rr == -3) {
			print_error("cat", str_msg_permission, argv[i]);
			ret = 1;
		} else if (rr == -4) {
			ret = 130;
			break;
		}

		if (key_check_ctrl_c() || ctrl_c_hit) {
			ret = 130;
			break;
		}
	}

	return ret;
}

int cmd_cd(int argc, char **argv) {
	if (!check_argc("cd", 0, 1, argc)) {
		return 1;
	}

	int is_dash = 0;
	const char *display;
	char norm[256];

	if (argc == 0) {
		display = "~";
		path_normalize(current_home, norm, 256);
	} else if (argv[0][0] == '-' && argv[0][1] == '\0') {
		if (oldpwd[0] == '\0') {
			print_error("cd", str_msg_oldpwd_not_set, 0);
			return 1;
		}

		is_dash = 1;
		display = oldpwd;
		path_normalize(oldpwd, norm, 256);
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

	if (ext2_check_perm(&inode, 1, 0) != 0) {
		print_error("cd", str_msg_permission, display);
		return 1;
	}

	for (int i = 0; i < 256; i++) {
		oldpwd[i] = cwd[i];

		if (cwd[i] == '\0') {
			break;
		}
	}

	for (int i = 0; i < 256; i++) {
		cwd[i] = norm[i];
		path_buf[i] = norm[i];

		if (norm[i] == '\0') {
			break;
		}
	}

	build_prompt();

	if (is_dash) {
		print(cwd);
		print_char('\n');
	}

	return 0;
}

int cmd_clear(int argc, char **argv) {
	(void)argv;

	if (check_argc("clear", 0, 0, argc)) {
		clear_screen();
		cursor_goto(0, 0);
		return 0;
	}

	return 1;
}

int cmd_count(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("count", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("count", 0, 0, argc - st.index)) {
		return 1;
	}

	print("task_a: ");
	print_uint((uint32_t)counter_a);
	print_char('\n');
	print("task_b: ");
	print_uint((uint32_t)counter_b);
	print_char('\n');
	return 0;
}

int cmd_echo(int argc, char **argv) {
	if (!check_argc("echo", 0, -1, argc)) {
		return 1;
	}

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

		if (ctrl_c_hit) {
			break;
		}
	}

	if (newline) {
		print_char('\n');
	}

	return 0;
}

int cmd_false(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return 1;
}

int cmd_help(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("help", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("help", 0, 1, argc - st.index)) {
		return 1;
	}

	if (argc - st.index == 0) {
		int max_len = 0;

		for (int i = 0; i < command_count; i++) {
			int len = 0;

			while (commands[i].name[len] != '\0') {
				len++;
			}

			if (len > max_len) {
				max_len = len;
			}
		}

		for (int i = 0; i < command_count; i++) {
			print(commands[i].name);

			int len = 0;
			while (commands[i].name[len] != '\0') {
				len++;
			}

			for (int j = len; j < max_len + 2; j++) {
				print_char(' ');
			}

			print(commands[i].summary);
			print_char('\n');

			if (ctrl_c_hit) {
				break;
			}
		}
	} else {
		const char *arg = argv[st.index];

		for (int i = 0; i < command_count; i++) {
			if (strcmp(arg, commands[i].name) == 0) {
				print(commands[i].name);
				print(" - ");
				print(commands[i].detail);
				print_char('\n');
				return 0;
			}
		}

		print_cmd_not_found("help", arg);
		return 1;
	}

	return 0;
}

int cmd_history(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("history", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("history", 0, 0, argc - st.index)) {
		return 1;
	}

	for (int i = 0; i < hist_count; i++) {
		print_char(' ');
		print_uint((uint32_t)(i + 1));
		print_char(' ');
		print(hist[i]);
		print_char('\n');

		if (ctrl_c_hit) {
			break;
		}
	}

	return 0;
}

int cmd_hostname(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("hostname", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("hostname", 0, 1, argc - st.index)) {
		return 1;
	}

	if (argc - st.index == 0) {
		print(host);
		print_char('\n');
		return 0;
	}

	if (current_euid != 0) {
		print_error("hostname", str_msg_permission, 0);
		return 1;
	}

	const char *new_name = argv[st.index];
	int len = 0;

	while (new_name[len] != '\0') {
		len++;
	}

	if (len == 0 || len >= HOST_NAME_MAX) {
		print_error("hostname", str_msg_invalid_name, new_name);
		return 1;
	}

	for (int i = 0; i <= len; i++) {
		host[i] = new_name[i];
	}

	static char write_buf[128];
	int w = 0;

	for (int i = 0; i < len && w < 126; i++) {
		write_buf[w++] = new_name[i];
	}
	write_buf[w++] = '\n';

	int wr = ext2_write_file("/etc/hostname", write_buf, (uint32_t)w, 0);

	if (wr != 0) {
		print_error("hostname", str_msg_cannot_write, "/etc/hostname");
		return 1;
	}

	build_prompt();
	return 0;
}

int cmd_id(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("id", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("id", 0, 0, argc - st.index)) {
		return 1;
	}

	uint32_t gid = 0;

	users_gid_by_uid(current_uid, &gid);

	print("uid=");
	print_uint(current_uid);
	print("(");
	print(current_user);
	print(") gid=");
	print_uint(gid);
	print_char('\n');
	return 0;
}

int cmd_ls(int argc, char **argv) {
	if (!check_argc("ls", 0, 4, argc)) {
		return 2;
	}

	int show_hidden = 0;
	int show_inode = 0;
	int show_long = 0;

	struct opt_state st = { 0, 0 };
	char opt;
	int r;

	while ((r = opt_next("ls", argc, argv, "ail", &st, &opt)) == 0) {
		if (opt == 'a') {
			show_hidden = 1;
		} else if (opt == 'i') {
			show_inode = 1;
		} else if (opt == 'l') {
			show_long = 1;
		}
	}

	if (r < 0) {
		return 2;
	}

	const char *path = 0;
	int path_count = 0;

	for (int i = st.index; i < argc; i++) {
		path = argv[i];
		path_count++;
	}

	if (path_count > 1) {
		print_error("ls", str_msg_too_many, 0);
		return 2;
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
		return 2;
	}

	struct ext2_inode inode;
	ext2_read_inode(ino, &inode);

	if ((inode.i_mode & 0xF000) != 0x4000) {
		const char *base = norm;

		for (const char *p = norm; *p != '\0'; p++) {
			if (*p == '/') {
				base = p + 1;
			}
		}

		if (show_inode) {
			print_uint(ino);
			print_char(' ');
		}

		if (show_long) {
			char perm[12];

			ext2_perm_str(inode.i_mode, perm);
			print(perm);
			print_char(' ');
			print_uint(inode.i_uid);
			print_char(' ');
			print_uint(inode.i_gid);
			print_char(' ');
			print_uint(inode.i_size);
			print_char(' ');
		}

		print(base);
		print_char('\n');
		return 0;
	}

	if (ext2_check_perm(&inode, 4, 0) != 0) {
		print_error("ls", str_msg_permission, display);
		return 2;
	}

	ext2_ls(norm, show_hidden, show_inode, show_long);
	return 0;
}

// 逐级创建目录，已存在的中间目录视为成功
// 成功返回 0，失败返回 ext2_mkdir 的错误码
static int mkdir_parents(const char *norm) {
	char tmp[256];
	int len = 0;

	while (norm[len] != '\0' && len < 255) {
		tmp[len] = norm[len];
		len++;
	}

	tmp[len] = '\0';

	for (int i = 1; i <= len; i++) {
		if (tmp[i] != '/' && tmp[i] != '\0') {
			continue;
		}

		char saved = tmp[i];

		tmp[i] = '\0';

		uint32_t ino;
		int rr;

		if (ext2_lookup(tmp, &ino) == 0) {
			struct ext2_inode in;

			ext2_read_inode(ino, &in);

			if ((in.i_mode & 0xF000) != 0x4000) {
				tmp[i] = saved;
				return -3;
			}

			rr = 0;
		} else {
			rr = ext2_mkdir(tmp);
		}

		tmp[i] = saved;

		if (rr != 0) {
			return rr;
		}
	}

	return 0;
}

int cmd_mkdir(int argc, char **argv) {
	int parents = 0;

	struct opt_state st = { 0, 0 };
	char opt;
	int r;

	while ((r = opt_next("mkdir", argc, argv, "p", &st, &opt)) == 0) {
		if (opt == 'p') {
			parents = 1;
		}
	}

	if (r < 0) {
		return 1;
	}

	if (!check_argc("mkdir", 1, -1, argc - st.index)) {
		return 1;
	}

	int ret = 0;

	for (int i = st.index; i < argc; i++) {
		char norm[256];

		build_abs_path(argv[i], norm, 256);

		int rr;

		if (parents) {
			rr = mkdir_parents(norm);
		} else {
			rr = ext2_mkdir(norm);
		}

		if (rr == 0) {
			if (ctrl_c_hit) {
				break;
			}

			continue;
		}

		if (rr == -1) {
			print_error("mkdir", str_msg_noent, argv[i]);
		} else if (rr == -2) {
			print_error("mkdir", str_msg_exists, argv[i]);
		} else if (rr == -3) {
			print_error("mkdir", str_msg_notdir, argv[i]);
		} else if (rr == -6) {
			print_error("mkdir", str_msg_permission, argv[i]);
		} else {
			print_error("mkdir", str_msg_nospace, argv[i]);
		}

		ret = 1;

		if (ctrl_c_hit) {
			break;
		}
	}

	return ret;
}
int cmd_passwd(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("passwd", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 2;
	}

	if (!check_argc("passwd", 0, 1, argc - st.index)) {
		return 2;
	}

	const char *target_name;
	int is_self = 1;

	if (argc - st.index == 0) {
		target_name = current_user;
	} else {
		target_name = argv[st.index];

		int j = 0;
		int same = 1;

		while (current_user[j] != '\0' || target_name[j] != '\0') {
			if (current_user[j] != target_name[j]) {
				same = 0;
				break;
			}

			j++;
		}

		is_self = same;
	}

	if (!is_self && current_euid != 0) {
		print_error("passwd", str_msg_permission, target_name);
		return 1;
	}

	uint32_t uid;

	if (users_uid_by_name(target_name, &uid) != 0) {
		print_error("passwd", str_msg_user_not_found, target_name);
		return 1;
	}

	if (is_self && current_euid != 0) {
		if (check_password("passwd", target_name) != 0) {
			return 1;
		}
	}

	print(str_new_password_prompt);

	char new1[USER_PASS_MAX];
	if (read_password(new1, USER_PASS_MAX) < 0) {
		return 1;
	}

	print(str_retype_password_prompt);

	char new2[USER_PASS_MAX];
	if (read_password(new2, USER_PASS_MAX) < 0) {
		return 1;
	}

	if (strcmp(new1, new2) != 0) {
		print_error("passwd", str_msg_password_mismatch, 0);
		return 1;
	}

	int idx = -1;

	for (int i = 0; i < users_count; i++) {
		int j = 0;
		int match = 1;

		while (users[i].name[j] != '\0' || target_name[j] != '\0') {
			if (users[i].name[j] != target_name[j]) {
				match = 0;
				break;
			}

			j++;
		}

		if (match) {
			idx = i;
			break;
		}
	}

	if (idx < 0) {
		print_error("passwd", str_msg_user_not_found, target_name);
		return 1;
	}

	for (int k = 0; k < USER_PASS_MAX; k++) {
		users[idx].password[k] = new1[k];

		if (new1[k] == '\0') {
			break;
		}
	}

	users_save();
	return 0;
}

int cmd_pwd(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("pwd", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("pwd", 0, 0, argc - st.index)) {
		return 1;
	}

	print(cwd);
	print_char('\n');
	return 0;
}

int cmd_reboot(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("reboot", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("reboot", 0, 0, argc - st.index)) {
		return 1;
	}

	if (current_euid != 0) {
		print_error("reboot", str_msg_permission, 0);
		return 1;
	}

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0xFE), "d"((uint16_t)0x64));

	while (1) {
		__asm__ volatile ("hlt");
	}

	return 1;
}

int cmd_rm(int argc, char **argv) {
	int recursive = 0;
	int force = 0;

	struct opt_state st = { 0, 0 };
	char opt;
	int r;

	while ((r = opt_next("rm", argc, argv, "rf", &st, &opt)) == 0) {
		if (opt == 'r') {
			recursive = 1;
		} else if (opt == 'f') {
			force = 1;
		}
	}

	if (r < 0) {
		return 1;
	}

	if (!check_argc("rm", 1, -1, argc - st.index)) {
		return 1;
	}

	int ret = 0;

	for (int i = st.index; i < argc; i++) {
		char norm[256];
		build_abs_path(argv[i], norm, 256);

		int rr = ext2_remove(norm, recursive);

		if (rr == 0) {
			if (ctrl_c_hit) {
				break;
			}

			continue;
		}

		if (rr == -2 && force) {
			continue;
		}

		if (rr == -1 || rr == -2) {
			print_error("rm", str_msg_noent, argv[i]);
		} else if (rr == -3) {
			print_error("rm", str_msg_notdir, argv[i]);
		} else if (rr == -4) {
			print_error("rm", str_msg_isdir, argv[i]);
		} else if (rr == -6) {
			print_error("rm", str_msg_permission, argv[i]);
		}

		ret = 1;

		if (ctrl_c_hit) {
			break;
		}
	}

	return ret;
}

int cmd_rmdir(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("rmdir", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("rmdir", 1, -1, argc - st.index)) {
		return 1;
	}

	int ret = 0;

	for (int i = st.index; i < argc; i++) {
		char norm[256];
		build_abs_path(argv[i], norm, 256);

		int rr = ext2_rmdir(norm);

		if (rr == 0) {
			if (ctrl_c_hit) {
				break;
			}

			continue;
		}

		if (rr == -1 || rr == -2) {
			print_error("rmdir", str_msg_noent, argv[i]);
		} else if (rr == -3 || rr == -4) {
			print_error("rmdir", str_msg_notdir, argv[i]);
		} else if (rr == -6) {
			print_error("rmdir", str_msg_permission, argv[i]);
		} else {
			print_error("rmdir", str_msg_notempty, argv[i]);
		}

		ret = 1;

		if (ctrl_c_hit) {
			break;
		}
	}

	return ret;
}

int cmd_stat(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("stat", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("stat", 1, 1, argc - st.index)) {
		return 1;
	}

	char norm[256];
	build_abs_path(argv[st.index], norm, 256);

	uint32_t ino;

	if (ext2_lookup(norm, &ino) != 0) {
		print_error("stat", str_msg_noent, argv[st.index]);
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
	print(argv[st.index]);
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

	char perm[12];
	ext2_perm_str(inode.i_mode, perm);

	print("  Mode:  ");
	print(perm);
	print("          Uid: ");
	print_uint(inode.i_uid);
	print("          Gid: ");
	print_uint(inode.i_gid);
	print_char('\n');

	return 0;
}

int cmd_su(int argc, char **argv) {
	int login = 0;
	int name_count = 0;
	const char *target_name = "root";

	struct opt_state st = { 0, 0 };
	char opt;
	int r;

	while ((r = opt_next("su", argc, argv, "l", &st, &opt)) == 0) {
		if (opt == 'l') {
			login = 1;
		}
	}

	if (r < 0) {
		return 2;
	}

	if (st.index < argc) {
		const char *tok = argv[st.index];

		if (tok[0] == '-' && tok[1] == '\0') {
			login = 1;
			st.index++;
		}
	}

	for (int i = st.index; i < argc; i++) {
		target_name = argv[i];
		name_count++;
	}

	if (name_count > 1) {
		print_error("su", str_msg_too_many, 0);
		return 2;
	}

	uint32_t uid;

	if (users_uid_by_name(target_name, &uid) != 0) {
		print_error("su", str_msg_user_not_found, target_name);
		return 1;
	}

	if (current_euid != 0) {
		if (check_password("su", target_name) != 0) {
			return 1;
		}
	}

	current_uid = uid;
	current_euid = uid;

	uint32_t gid = 0;
	users_gid_by_uid(uid, &gid);
	current_egid = gid;

	if (users_name_by_uid(uid, current_user, 32) != 0) {
		print_error("su", str_msg_user_not_found, target_name);
		return 1;
	}

	if (users_home_by_uid(uid, current_home, 256) != 0) {
		print_error("su", str_msg_user_not_found, target_name);
		return 1;
	}

	if (login) {
		path_normalize(current_home, cwd, 256);
		path_normalize(current_home, path_buf, 256);
	}

	build_prompt();
	return 0;
}

int cmd_sudo(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("sudo", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 2;
	}

	if (argc - st.index < 1) {
		return 2;
	}

	if (current_euid != 0) {
		if (check_password("sudo", current_user) != 0) {
			return 1;
		}
	}

	const char *cmd = argv[st.index];

	for (int i = 0; i < command_count; i++) {
		if (strcmp(cmd, commands[i].name) == 0) {
			uint32_t saved_euid = current_euid;
			uint32_t saved_egid = current_egid;
			current_euid = 0;
			current_egid = 0;

			int ret = commands[i].func(argc - st.index - 1, argv + st.index + 1);

			current_euid = saved_euid;
			current_egid = saved_egid;
			return ret;
		}
	}

	print_error("sudo", str_msg_cmd_not_found, cmd);
	return 127;
}

int cmd_touch(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("touch", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("touch", 1, -1, argc - st.index)) {
		return 1;
	}

	int ret = 0;

	for (int i = st.index; i < argc; i++) {
		char norm[256];

		build_abs_path(argv[i], norm, 256);

		uint32_t existing;

		if (ext2_lookup(norm, &existing) == 0) {
			struct ext2_inode tmp;

			ext2_read_inode(existing, &tmp);

			if ((tmp.i_mode & 0xF000) == 0x4000) {
				print_error("touch", str_msg_isdir, argv[i]);
				ret = 1;
			} else {
				uint32_t now = rtc_get_unix_time();

				tmp.i_atime = now;
				tmp.i_mtime = now;
				tmp.i_ctime = now;
				ext2_write_inode(existing, &tmp);
			}

			if (ctrl_c_hit) {
				break;
			}

			continue;
		}

		int rr = ext2_write_file(norm, "", 0, 0);

		if (rr == -1) {
			print_error("touch", str_msg_noent, argv[i]);
			ret = 1;
		} else if (rr == -2) {
			print_error("touch", str_msg_noent, argv[i]);
			ret = 1;
		} else if (rr == -3) {
			print_error("touch", str_msg_notdir, argv[i]);
			ret = 1;
		} else if (rr == -4) {
			print_error("touch", str_msg_isdir, argv[i]);
			ret = 1;
		} else if (rr == -6) {
			print_error("touch", str_msg_permission, argv[i]);
			ret = 1;
		} else if (rr == -5) {
			print_error("touch", str_msg_nospace, argv[i]);
			ret = 1;
		}

		if (ctrl_c_hit) {
			break;
		}
	}

	return ret;
}

int cmd_tree(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("tree", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("tree", 0, 1, argc - st.index)) {
		return 1;
	}

	char norm[256];
	const char *display;

	if (argc - st.index == 0) {
		display = ".";
		for (int i = 0; i < 256; i++) {
			norm[i] = cwd[i];

			if (cwd[i] == '\0') {
				break;
			}
		}
	} else {
		display = argv[st.index];
		build_abs_path(argv[st.index], norm, 256);
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

	if (ext2_check_perm(&inode, 4, 0) != 0) {
		print_error("tree", str_msg_permission, display);
		return 1;
	}

	print(display);
	print_char('\n');
	ext2_tree(norm, 0);
	return 0;
}

int cmd_true(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return 0;
}

int cmd_uname(int argc, char **argv) {
	if (!check_argc("uname", 0, -1, argc)) {
		return 1;
	}

	int show_s = 0;
	int show_n = 0;
	int show_r = 0;
	int show_v = 0;
	int show_m = 0;
	int show_o = 0;

	if (argc == 0) {
		show_s = 1;
	} else {
		struct opt_state st = { 0, 0 };
		char opt;
		int r;

		while ((r = opt_next("uname", argc, argv, "asnrvmo", &st, &opt)) == 0) {
			if (opt == 'a') {
				show_s = 1;
				show_n = 1;
				show_r = 1;
				show_v = 1;
				show_m = 1;
				show_o = 1;
			} else if (opt == 's') {
				show_s = 1;
			} else if (opt == 'n') {
				show_n = 1;
			} else if (opt == 'r') {
				show_r = 1;
			} else if (opt == 'v') {
				show_v = 1;
			} else if (opt == 'm') {
				show_m = 1;
			} else if (opt == 'o') {
				show_o = 1;
			}
		}

		if (r < 0) {
			return 1;
		}
	}

	int first = 1;

	if (show_s) {
		if (!first) {
			print_char(' ');
		}

		const char *name = OS_NAME;
		int len = 0;

		while (name[len] != '\0') {
			len++;
		}

		int kern_len = len;

		if (kern_len >= 2 && name[kern_len - 1] == 'S' && name[kern_len - 2] == 'O') {
			kern_len -= 2;
		}

		for (int i = 0; i < kern_len; i++) {
			print_char(name[i]);
		}

		first = 0;
	}

	if (show_n) {
		if (!first) {
			print_char(' ');
		}

		print(host);
		first = 0;
	}

	if (show_r) {
		if (!first) {
			print_char(' ');
		}

		print(OS_VERSION);
		first = 0;
	}

	if (show_v) {
		if (!first) {
			print_char(' ');
		}

		print(__DATE__);
		print_char(' ');
		print(__TIME__);
		first = 0;
	}

	if (show_m) {
		if (!first) {
			print_char(' ');
		}

		print(OS_ARCH);
		print_char('_');
		print(OS_BITS);
		first = 0;
	}

	if (show_o) {
		if (!first) {
			print_char(' ');
		}

		print(OS_NAME);
		first = 0;
	}

	print_char('\n');
	return 0;
}

int cmd_uptime(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("uptime", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("uptime", 0, 0, argc - st.index)) {
		return 1;
	}

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

int cmd_useradd(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("useradd", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 2;
	}

	if (!check_argc("useradd", 1, 1, argc - st.index)) {
		return 2;
	}

	if (current_euid != 0) {
		print_error("useradd", str_msg_permission, 0);
		return 1;
	}

	const char *name = argv[st.index];

	int rr = users_add(name);

	if (rr != 0) {
		if (rr == -1) {
			print_error("useradd", str_msg_user_exists, name);
			return 9;
		} else if (rr == -2) {
			print_error("useradd", str_msg_too_many_users, 0);
			return 1;
		} else {
			print_error("useradd", str_msg_invalid_name, name);
			return 3;
		}
	}

	uint32_t uid;

	if (users_uid_by_name(name, &uid) != 0) {
		users_del(name);
		return 1;
	}

	char home[256];

	if (users_home_by_uid(uid, home, 256) != 0) {
		users_del(name);
		return 1;
	}

	uint32_t ino;
	int exists = (ext2_lookup(home, &ino) == 0);

	if (exists) {
		struct ext2_inode hino;
		ext2_read_inode(ino, &hino);

		if ((hino.i_mode & 0xF000) != 0x4000) {
			print_error("useradd", str_msg_cannot_create_dir, home);
			users_del(name);
			return 12;
		}
	} else {
		int mr = ext2_mkdir(home);

		if (mr != 0) {
			if (mr == -6) {
				print_error("useradd", str_msg_permission, home);
			} else {
				print_error("useradd", str_msg_cannot_create_dir, home);
			}

			users_del(name);
			return 12;
		}

		if (ext2_lookup(home, &ino) != 0) {
			users_del(name);
			return 1;
		}
	}

	struct ext2_inode hino;
	ext2_read_inode(ino, &hino);
	hino.i_uid = (uint16_t)uid;
	hino.i_gid = (uint16_t)uid;
	ext2_write_inode(ino, &hino);

	return 0;
}

int cmd_userdel(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("userdel", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 2;
	}

	if (!check_argc("userdel", 1, 1, argc - st.index)) {
		return 2;
	}

	if (current_euid != 0) {
		print_error("userdel", str_msg_permission, 0);
		return 1;
	}

	const char *name = argv[st.index];

	int is_root = 1;
	int j = 0;

	while (name[j] != '\0' || "root"[j] != '\0') {
		if (name[j] != "root"[j]) {
			is_root = 0;
			break;
		}

		j++;
	}

	if (is_root) {
		print_error("userdel", str_msg_cannot_remove_root, "root");
		return 1;
	}

	int is_current = 1;
	j = 0;

	while (current_user[j] != '\0' || name[j] != '\0') {
		if (current_user[j] != name[j]) {
			is_current = 0;
			break;
		}

		j++;
	}

	if (is_current) {
		print_error("userdel", str_msg_cannot_remove_current, name);
		return 8;
	}

	int rr = users_del(name);

	if (rr == 0) {
		return 0;
	}

	print_error("userdel", str_msg_user_not_found, name);
	return 6;
}

int cmd_usermod(int argc, char **argv) {
	int i = 0;
	int got_l = 0;
	const char *new_name = 0;
	const char *old_name = 0;

	while (i < argc) {
		const char *tok = argv[i];

		if (tok[0] != '-') {
			if (old_name != 0) {
				print_error("usermod", str_msg_too_many, 0);
				return 2;
			}

			old_name = tok;
			i++;
			continue;
		}

		if (tok[1] == '\0') {
			print_error("usermod", str_msg_invalid_option, tok);
			return 2;
		}

		if (tok[1] == 'l' && tok[2] == '\0') {
			if (i + 1 >= argc) {
				print_error("usermod", str_msg_option_requires_arg, tok);
				return 2;
			}

			got_l = 1;
			new_name = argv[i + 1];
			i += 2;
			continue;
		}

		print_error("usermod", str_msg_invalid_option, tok);
		return 2;
	}

	if (old_name == 0) {
		print_error("usermod", str_msg_too_few, 0);
		return 2;
	}

	if (!got_l) {
		print_error("usermod", str_msg_no_changes, 0);
		return 0;
	}

	if (current_euid != 0) {
		print_error("usermod", str_msg_permission, 0);
		return 1;
	}

	if (strcmp(old_name, current_user) == 0) {
		print_error("usermod", str_msg_cannot_rename_current, old_name);
		return 8;
	}

	int rr = users_rename(old_name, new_name);

	if (rr == 0) {
		return 0;
	}

	if (rr == -1) {
		print_error("usermod", str_msg_user_not_found, old_name);
		return 6;
	} else if (rr == -2) {
		print_error("usermod", str_msg_user_exists, new_name);
		return 9;
	} else if (rr == -3) {
		print_error("usermod", str_msg_invalid_name, new_name);
		return 3;
	}

	return 1;
}

int cmd_users(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("users", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("users", 0, 0, argc - st.index)) {
		return 1;
	}

	for (int i = 0; i < users_count; i++) {
		print(users[i].name);
		print_char('\n');

		if (ctrl_c_hit) {
			break;
		}
	}

	return 0;
}

int cmd_whoami(int argc, char **argv) {
	struct opt_state st = { 0, 0 };
	char opt;
	int r = opt_next("whoami", argc, argv, "", &st, &opt);

	if (r < 0) {
		return 1;
	}

	if (!check_argc("whoami", 0, 0, argc - st.index)) {
		return 1;
	}

	print(current_user);
	print_char('\n');
	return 0;
}

int cmd_write(int argc, char **argv) {
	int append = 0;
	int newline = 1;

	struct opt_state st = { 0, 0 };
	char opt;
	int r;

	while ((r = opt_next("write", argc, argv, "an", &st, &opt)) == 0) {
		if (opt == 'a') {
			append = 1;
		} else if (opt == 'n') {
			newline = 0;
		}
	}

	if (r < 0) {
		return 1;
	}

	if (!check_argc("write", 1, -1, argc - st.index)) {
		return 1;
	}

	const char *file = argv[st.index];
	char norm[256];
	build_abs_path(file, norm, 256);

	static char buf[49152];
	int w = 0;

	for (int i = st.index + 1; i < argc; i++) {
		if (i > st.index + 1) {
			if (w < 49150) {
				buf[w++] = ' ';
			}
		}

		for (int j = 0; argv[i][j] != '\0' && w < 49150; j++) {
			buf[w++] = argv[i][j];

			if (ctrl_c_hit) {
				break;
			}
		}

		if (ctrl_c_hit) {
			break;
		}
	}

	if (newline) {
		buf[w++] = '\n';
	}

	int wr = ext2_write_file(norm, buf, (uint32_t)w, append);

	if (wr == 0) {
		return 0;
	}

	if (wr == -1) {
		print_error("write", str_msg_noent, file);
	} else if (wr == -2) {
		print_error("write", str_msg_noent, file);
	} else if (wr == -3) {
		print_error("write", str_msg_notdir, file);
	} else if (wr == -4) {
		print_error("write", str_msg_isdir, file);
	} else if (wr == -6) {
		print_error("write", str_msg_permission, file);
	} else {
		print_error("write", str_msg_nospace, file);
	}

	return 1;
}