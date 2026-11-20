#include <stdint.h>
#include "kernel.h"
#include "login.h"
#include "shell.h"
#include "users.h"
#include "ext2.h"
#include "strings.h"

#define LOGIN_LINE_MAX	256

// 初始化标志文件与标志值，仿 systemd machine-id
#define INIT_FLAG_PATH		"/etc/machine-id"
#define INIT_FLAG_UNINIT	"uninitialized"

// 读一行，回显，末尾加 '\0'，返回长度
static int read_line(char *buf, int max) {
	int len = 0;

	while (1) {
		__asm__ volatile ("hlt");

		handle_cursor_blink();

		char c = get_key();
		if (c == 0) {
			continue;
		}

		if (c == 0x0C) {
			print_char('^');
			print_char('C');
			print_char('\n');
			return -1;
		}

		if (c == '\n') {
			buf[len] = '\0';
			print_char('\n');
			return len;
		}

		if (c == '\b') {
			if (len > 0) {
				len--;
				print_char('\b');
				print_char(' ');
				print_char('\b');
			}
		} else {
			if (len < max - 1) {
				buf[len++] = c;
				print_char(c);
			}
		}
	}
}

// 把当前身份设为指定 UID 对应用户
static void set_identity(uint32_t uid) {
	current_uid = uid;
	current_euid = uid;

	uint32_t gid = 0;
	users_gid_by_uid(uid, &gid);
	current_egid = gid;

	users_name_by_uid(uid, current_user, 32);
	users_home_by_uid(uid, current_home, 256);

	path_normalize(current_home, cwd, 256);
	path_normalize(current_home, path_buf, 256);
}

// 生成 32 位十六进制机器 ID，写入 buf，buf 至少 33 字节
static void generate_machine_id(char *buf) {
	uint64_t seed = ticks ^ ((uint64_t)fb_addr << 16)
		^ ((uint64_t)fb_width << 8) ^ (uint64_t)fb_height;

	if (seed == 0) {
		seed = 0x123456789ABCDEF0ULL;
	}

	const char hex[] = "0123456789abcdef";

	for (int i = 0; i < 32; i++) {
		buf[i] = hex[seed & 0xF];
		seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
		seed ^= seed >> 33;
	}

	buf[32] = '\0';
}

// 判断是否为首次启动
// 文件不存在，或内容等于 INIT_FLAG_UNINIT，都算首次
static int is_first_boot(void) {
	static char buf[64];

	int r = ext2_read_file(INIT_FLAG_PATH, buf, 63);

	if (r <= 0) {
		return 1;
	}

	int n = r;
	if (n > 63) {
		n = 63;
	}
	buf[n] = '\0';

	if (strcmp(buf, INIT_FLAG_UNINIT) == 0) {
		return 1;
	}

	return 0;
}

// 首次启动，设置 root 密码，写机器 ID
static void first_boot(void) {
	clear_screen();
	cursor_goto(0, 0);
	print(str_first_boot_header);

	char pw1[USER_PASS_MAX];
	char pw2[USER_PASS_MAX];

	while (1) {
		print(str_new_password_prompt);
		if (read_password(pw1, USER_PASS_MAX) < 0) {
			ctrl_c_hit = 0;
			ctrl_c_reset();
			continue;
		}

		print(str_retype_password_prompt);
		if (read_password(pw2, USER_PASS_MAX) < 0) {
			ctrl_c_hit = 0;
			ctrl_c_reset();
			continue;
		}

		if (strcmp(pw1, pw2) == 0) {
			break;
		}

		print(str_password_mismatch_retry);
	}

	int idx = -1;

	for (int i = 0; i < users_count; i++) {
		if (users[i].uid == 0) {
			idx = i;
			break;
		}
	}

	if (idx >= 0) {
		for (int k = 0; k < USER_PASS_MAX; k++) {
			users[idx].password[k] = pw1[k];

			if (pw1[k] == '\0') {
				break;
			}
		}

		users_save();
	}

	char machine_id[33];
	generate_machine_id(machine_id);

	ext2_write_file(INIT_FLAG_PATH, machine_id, 32, 0);

	set_identity(0);

	print(str_root_password_set);
}

// 登录循环，密码为空跳过 Password 提示，密码错误直接换行重试
static void do_login(void) {
	clear_screen();
	cursor_goto(0, 0);
	while (1) {
		char name[USER_NAME_MAX];

		print(host);
		print(str_login_suffix);
		if (read_line(name, USER_NAME_MAX) < 0) {
			continue;
		}

		if (name[0] == '\0') {
			continue;
		}

		uint32_t uid;

		if (users_uid_by_name(name, &uid) != 0) {
			print(str_login_incorrect);
			continue;
		}

		char expected[USER_PASS_MAX];

		if (users_password_by_name(name, expected, USER_PASS_MAX) != 0) {
			print(str_login_incorrect);
			continue;
		}

		if (expected[0] != '\0') {
			char pw[USER_PASS_MAX];

			print(str_password_prompt);
			if (read_password(pw, USER_PASS_MAX) < 0) {
				ctrl_c_hit = 0;
				ctrl_c_reset();
				continue;
			}

			if (strcmp(pw, expected) != 0) {
				print(str_login_incorrect);
				continue;
			}
		}

		set_identity(uid);
		return;
	}
}

// 登录模块入口，由 kernel_main 调用
void login_init(void) {
	users_load();

	static char host_buf[128];
	int hr = ext2_read_file("/etc/hostname", host_buf, 127);

	if (hr > 0) {
		int n = hr;
		if (n > 127) {
			n = 127;
		}
		host_buf[n] = '\0';

		while (n > 0 && (host_buf[n - 1] == '\n' || host_buf[n - 1] == '\r')) {
			host_buf[n - 1] = '\0';
			n--;
		}

		if (n > 0 && n < HOST_NAME_MAX) {
			for (int i = 0; i <= n; i++) {
				host[i] = host_buf[i];
			}
		}
	}

	if (is_first_boot()) {
		first_boot();
	} else {
		do_login();
	}
}