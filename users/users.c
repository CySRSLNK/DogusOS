#include <stdint.h>
#include "kernel.h"
#include "ext2.h"
#include "users.h"
#include "strings.h"

struct user_entry users[USERS_MAX];
int users_count = 0;

#define PASSWD_BUF_SIZE	8192
#define SHADOW_BUF_SIZE	8192

static char passwd_buf[PASSWD_BUF_SIZE];
static char shadow_buf[SHADOW_BUF_SIZE];

static int u32_to_str(uint32_t v, char *buf) {
	char tmp[16];
	int t = 0;

	if (v == 0) {
		buf[0] = '0';
		return 1;
	}

	while (v > 0 && t < 15) {
		tmp[t++] = '0' + (v % 10);
		v /= 10;
	}

	int w = 0;

	while (t > 0) {
		buf[w++] = tmp[--t];
	}

	return w;
}

static int parse_passwd_line(const char *line, int len, struct user_entry *out) {
	int field = 0;
	int i = 0;

	char name[USER_NAME_MAX];
	int name_len = 0;

	uint32_t uid = 0;
	uint32_t gid = 0;

	char home[USER_HOME_MAX];
	int home_len = 0;

	int has_uid = 0;
	int has_gid = 0;

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
				has_uid = 1;
			}

			i++;
		} else if (field == 3) {
			if (line[i] == ':') {
				field = 4;
				i++;
				continue;
			}

			if (line[i] >= '0' && line[i] <= '9') {
				gid = gid * 10 + (uint32_t)(line[i] - '0');
				has_gid = 1;
			}

			i++;
		} else if (field == 4) {
			if (line[i] == ':') {
				field = 5;
			}

			i++;
		} else if (field == 5) {
			if (line[i] == ':') {
				field = 6;
				i++;
				continue;
			}

			if (home_len < USER_HOME_MAX - 1) {
				home[home_len++] = line[i];
			}

			i++;
		} else {
			i++;
		}
	}

	if (name_len == 0 || !has_uid || !has_gid) {
		return -1;
	}

	name[name_len] = '\0';
	home[home_len] = '\0';

	for (int j = 0; j <= name_len; j++) {
		out->name[j] = name[j];
	}

	for (int j = 0; j <= home_len; j++) {
		out->home[j] = home[j];
	}

	out->uid = uid;
	out->gid = gid;
	out->password[0] = '\0';

	return 0;
}

static int find_user_index(const char *name) {
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
			return i;
		}
	}

	return -1;
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

			if (parse_passwd_line(passwd_buf + start, len, &e) == 0) {
				users[users_count++] = e;
			}
		}

		if (i < r) {
			i++;
		}
	}

	int sr = ext2_read_file("/etc/shadow", shadow_buf, SHADOW_BUF_SIZE - 1);

	if (sr > 0) {
		int j = 0;

		while (j < sr) {
			int start = j;

			while (j < sr && shadow_buf[j] != '\n') {
				j++;
			}

			int len = j - start;

			if (len > 0) {
				char name[USER_NAME_MAX];
				int name_len = 0;
				char pass[USER_PASS_MAX];
				int pass_len = 0;
				int field = 0;
				int k = start;

				while (k < j) {
					if (field == 0) {
						if (shadow_buf[k] == ':') {
							field = 1;
							k++;
							continue;
						}

						if (name_len < USER_NAME_MAX - 1) {
							name[name_len++] = shadow_buf[k];
						}

						k++;
					} else if (field == 1) {
						if (shadow_buf[k] == ':') {
							field = 2;
							k++;
							continue;
						}

						if (pass_len < USER_PASS_MAX - 1) {
							pass[pass_len++] = shadow_buf[k];
						}

						k++;
					} else {
						k++;
					}
				}

				name[name_len] = '\0';
				pass[pass_len] = '\0';

				if (name_len > 0) {
					int idx = find_user_index(name);

					if (idx >= 0) {
						for (int m = 0; m <= pass_len; m++) {
							users[idx].password[m] = pass[m];
						}
					}
				}
			}

			if (j < sr) {
				j++;
			}
		}
	}

	return users_count;
}

int users_save(void) {
	static char out[PASSWD_BUF_SIZE];
	int w = 0;

	for (int i = 0; i < users_count; i++) {
		const char *name = users[i].name;
		int j = 0;

		while (name[j] != '\0' && w < PASSWD_BUF_SIZE - 512) {
			out[w++] = name[j];
			j++;
		}

		out[w++] = ':';
		out[w++] = 'x';
		out[w++] = ':';

		{
			char tmp[16];
			int n = u32_to_str(users[i].uid, tmp);

			for (int k = 0; k < n; k++) {
				out[w++] = tmp[k];
			}
		}

		out[w++] = ':';

		{
			char tmp[16];
			int n = u32_to_str(users[i].gid, tmp);

			for (int k = 0; k < n; k++) {
				out[w++] = tmp[k];
			}
		}

		out[w++] = ':';
		out[w++] = ':';

		j = 0;
		while (users[i].home[j] != '\0' && w < PASSWD_BUF_SIZE - 32) {
			out[w++] = users[i].home[j];
			j++;
		}

		out[w++] = ':';
		out[w++] = '\n';
	}

	int r = ext2_write_file("/etc/passwd", out, (uint32_t)w, 0);

	if (r != 0) {
		return r;
	}

	static char sout[SHADOW_BUF_SIZE];
	w = 0;

	for (int i = 0; i < users_count; i++) {
		const char *name = users[i].name;
		int j = 0;

		while (name[j] != '\0' && w < SHADOW_BUF_SIZE - 256) {
			sout[w++] = name[j];
			j++;
		}

		sout[w++] = ':';

		j = 0;
		while (users[i].password[j] != '\0' && w < SHADOW_BUF_SIZE - 32) {
			sout[w++] = users[i].password[j];
			j++;
		}

		sout[w++] = ':';
		sout[w++] = '\n';
	}

	return ext2_write_file("/etc/shadow", sout, (uint32_t)w, 0);
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
	int idx = find_user_index(name);

	if (idx < 0) {
		return -1;
	}

	*uid = users[idx].uid;
	return 0;
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

int users_home_by_uid(uint32_t uid, char *home, int max) {
	for (int i = 0; i < users_count; i++) {
		if (users[i].uid == uid) {
			int j = 0;

			while (users[i].home[j] != '\0' && j < max - 1) {
				home[j] = users[i].home[j];
				j++;
			}

			home[j] = '\0';
			return 0;
		}
	}

	return -1;
}

int users_password_by_name(const char *name, char *password, int max) {
	int idx = find_user_index(name);

	if (idx < 0) {
		return -1;
	}

	int j = 0;

	while (users[idx].password[j] != '\0' && j < max - 1) {
		password[j] = users[idx].password[j];
		j++;
	}

	password[j] = '\0';
	return 0;
}

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

	if (find_user_index(name) >= 0) {
		return -1;
	}

	if (users_count >= USERS_MAX) {
		return -2;
	}

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
	e.password[0] = '\0';

	{
		const char *prefix = "/home/";
		int w = 0;

		while (prefix[w] != '\0' && w < USER_HOME_MAX - 1) {
			e.home[w] = prefix[w];
			w++;
		}

		for (int i = 0; i < n && w < USER_HOME_MAX - 1; i++) {
			e.home[w++] = name[i];
		}

		e.home[w] = '\0';
	}

	users[users_count++] = e;

	users_save();

	return 0;
}

int users_del(const char *name) {
	int idx = find_user_index(name);

	if (idx < 0) {
		return -1;
	}

	for (int k = idx; k < users_count - 1; k++) {
		users[k] = users[k + 1];
	}

	users_count--;

	users_save();
	return 0;
}

// 修改用户名，成功返回 0
// -1 旧用户不存在，-2 新名已存在，-3 新名非法
int users_rename(const char *old_name, const char *new_name) {
	int idx = find_user_index(old_name);

	if (idx < 0) {
		return -1;
	}

	int n = 0;
	while (new_name[n] != '\0') {
		n++;
	}

	if (n == 0 || n >= USER_NAME_MAX) {
		return -3;
	}

	for (int i = 0; i < n; i++) {
		if (new_name[i] == ':') {
			return -3;
		}
	}

	if (find_user_index(new_name) >= 0) {
		return -2;
	}

	for (int i = 0; i <= n; i++) {
		users[idx].name[i] = new_name[i];
	}

	users_save();
	return 0;
}