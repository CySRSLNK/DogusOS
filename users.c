#include <stdint.h>
#include "kernel.h"
#include "ext2.h"
#include "users.h"
#include "strings.h"

struct user_entry users[USERS_MAX];
int users_count = 0;

#define PASSWD_BUF_SIZE	4096

static char passwd_buf[PASSWD_BUF_SIZE];

// 从缓冲区解析一行，成功返回 0 并写入 out，失败返回 -1
static int parse_line(const char *line, int len, struct user_entry *out) {
	int field = 0;
	int i = 0;

	char name[USER_NAME_MAX];
	int name_len = 0;

	uint32_t uid = 0;
	uint32_t gid = 0;

	while (i < len) {
		if (line[i] == '\n') {
			break;
		}

		if (field == 0) {
			if (line[i] == ':') {
				field = 1;
				i++;
				continue;
			}

			if (name_len < USER_NAME_MAX - 1) {
				name[name_len++] = line[i];
			}

			i++;
		} else if (field == 1) {
			if (line[i] == ':') {
				field = 2;
			}

			i++;
		} else if (field == 2) {
			if (line[i] == ':') {
				field = 3;
				i++;
				continue;
			}

			if (line[i] >= '0' && line[i] <= '9') {
				uid = uid * 10 + (uint32_t)(line[i] - '0');
			}

			i++;
		} else if (field == 3) {
			if (line[i] == ':') {
				field = 4;
			}

			if (line[i] >= '0' && line[i] <= '9') {
				gid = gid * 10 + (uint32_t)(line[i] - '0');
			}

			i++;
		} else {
			i++;
		}
	}

	if (name_len == 0 || field < 3) {
		return -1;
	}

	name[name_len] = '\0';

	for (int j = 0; j < name_len; j++) {
		out->name[j] = name[j];
	}
	out->name[name_len] = '\0';
	out->uid = uid;
	out->gid = gid;

	return 0;
}

int users_load(void) {
	users_count = 0;

	int r = ext2_read_file("/etc/passwd", passwd_buf, PASSWD_BUF_SIZE - 1);

	if (r <= 0) {
		return -1;
	}

	int i = 0;

	while (i < r && users_count < USERS_MAX) {
		int start = i;

		while (i < r && passwd_buf[i] != '\n') {
			i++;
		}

		int len = i - start;

		if (len > 0) {
			struct user_entry e;

			if (parse_line(passwd_buf + start, len, &e) == 0) {
				users[users_count++] = e;
			}
		}

		if (i < r) {
			i++;
		}
	}

	return users_count;
}

// 把内存里的用户表写回 /etc/passwd
int users_save(void) {
	static char out[PASSWD_BUF_SIZE];
	int w = 0;

	for (int i = 0; i < users_count; i++) {
		const char *name = users[i].name;
		int j = 0;

		while (name[j] != '\0' && w < PASSWD_BUF_SIZE - 32) {
			out[w++] = name[j];
			j++;
		}

		out[w++] = ':';
		out[w++] = 'x';
		out[w++] = ':';

		// uid 十进制
		{
			char tmp[16];
			int t = 0;
			uint32_t v = users[i].uid;

			if (v == 0) {
				tmp[t++] = '0';
			} else {
				while (v > 0 && t < 15) {
					tmp[t++] = '0' + (v % 10);
					v /= 10;
				}
			}

			while (t > 0) {
				out[w++] = tmp[--t];
			}
		}

		out[w++] = ':';

		// gid 十进制
		{
			char tmp[16];
			int t = 0;
			uint32_t v = users[i].gid;

			if (v == 0) {
				tmp[t++] = '0';
			} else {
				while (v > 0 && t < 15) {
					tmp[t++] = '0' + (v % 10);
					v /= 10;
				}
			}

			while (t > 0) {
				out[w++] = tmp[--t];
			}
		}

		out[w++] = '\n';
	}

	return ext2_write_file("/etc/passwd", out, (uint32_t)w, 0);
}

int users_name_by_uid(uint32_t uid, char *name, int max) {
	for (int i = 0; i < users_count; i++) {
		if (users[i].uid == uid) {
			int j = 0;

			while (users[i].name[j] != '\0' && j < max - 1) {
				name[j] = users[i].name[j];
				j++;
			}

			name[j] = '\0';
			return 0;
		}
	}

	return -1;
}

int users_uid_by_name(const char *name, uint32_t *uid) {
	for (int i = 0; i < users_count; i++) {
		int j = 0;
		int match = 1;

		while (users[i].name[j] != '\0' || name[j] != '\0') {
			if (users[i].name[j] != name[j]) {
				match = 0;
				break;
			}

			j++;
		}

		if (match) {
			*uid = users[i].uid;
			return 0;
		}
	}

	return -1;
}

int users_gid_by_uid(uint32_t uid, uint32_t *gid) {
	for (int i = 0; i < users_count; i++) {
		if (users[i].uid == uid) {
			*gid = users[i].gid;
			return 0;
		}
	}

	return -1;
}

// 添加用户，分配下一个可用 UID，成功返回 0
// -1 用户已存在，-2 无空间，-3 名字非法
int users_add(const char *name) {
	int n = 0;
	while (name[n] != '\0') {
		n++;
	}

	if (n == 0 || n >= USER_NAME_MAX) {
		return -3;
	}

	for (int i = 0; i < n; i++) {
		if (name[i] == ':') {
			return -3;
		}
	}

	for (int i = 0; i < users_count; i++) {
		int j = 0;
		int match = 1;

		while (users[i].name[j] != '\0' || name[j] != '\0') {
			if (users[i].name[j] != name[j]) {
				match = 0;
				break;
			}

			j++;
		}

		if (match) {
			return -1;
		}
	}

	if (users_count >= USERS_MAX) {
		return -2;
	}

	// 找下一个可用 UID，从 1001 开始
	uint32_t uid = 1001;

	while (1) {
		int used = 0;

		for (int i = 0; i < users_count; i++) {
			if (users[i].uid == uid) {
				used = 1;
				break;
			}
		}

		if (!used) {
			break;
		}

		uid++;
	}

	struct user_entry e;

	for (int i = 0; i <= n; i++) {
		e.name[i] = name[i];
	}

	e.uid = uid;
	e.gid = uid;

	users[users_count++] = e;

	users_save();

	return 0;
}

// 删除用户，成功返回 0
// -1 用户不存在
int users_del(const char *name) {
	for (int i = 0; i < users_count; i++) {
		int j = 0;
		int match = 1;

		while (users[i].name[j] != '\0' || name[j] != '\0') {
			if (users[i].name[j] != name[j]) {
				match = 0;
				break;
			}

			j++;
		}

		if (match) {
			for (int k = i; k < users_count - 1; k++) {
				users[k] = users[k + 1];
			}

			users_count--;

			users_save();
			return 0;
		}
	}

	return -1;
}