#include <stdint.h>
#include "kernel.h"
#include "shell.h"
#include "commands.h"
#include "ext2.h"
#include "strings.h"

#define LINE_BUF_SIZE	256

char line_buf[LINE_BUF_SIZE];
int line_len = 0;
int line_cursor = 0;

int line_start_row = 0;
int line_old_rows = 0;
int line_old_len = 0;

#define KEY_LEFT	0x01
#define KEY_RIGHT	0x02
#define KEY_HOME	0x03
#define KEY_END		0x04
#define KEY_DELETE	0x05
#define KEY_UP		0x06
#define KEY_DOWN	0x07

const char *user = "user";
const char *host = "host";

char cwd[256] = "/";
char path_buf[256] = "/";

char prompt_buf[128] = { 0 };

char hist[HIST_SIZE][HIST_LINE_LEN];
int hist_count = 0;
int hist_pos = 0;
char hist_saved[LINE_BUF_SIZE];

int current_privilege = 0;
int last_exit_code = 0;

#define ARGV_MAX 512
#define GLOB_TOKEN_SIZE 256

void build_prompt(void) {
	int i = 0;

	while (user[i] != '\0' && i < 127) {
		prompt_buf[i] = user[i];
		i++;
	}

	if (i < 127) {
		prompt_buf[i++] = '@';
	}

	int j = 0;
	while (host[j] != '\0' && i < 127) {
		prompt_buf[i++] = host[j];
		j++;
	}

	if (i < 127) {
		prompt_buf[i++] = ':';
	}

	j = 0;
	while (path_buf[j] != '\0' && i < 127) {
		prompt_buf[i++] = path_buf[j];
		j++;
	}

	if (i < 126) {
		prompt_buf[i++] = '$';
		prompt_buf[i++] = ' ';
	}

	prompt_buf[i] = '\0';
}

// 归一化路径，以 / 开头，不以 / 结尾（根目录除外）
void path_normalize(const char *in, char *out, int out_size) {
	char *stack[64];
	int depth = 0;
	char buf[256];
	int n = 0;

	while (*in != '\0' && n < 255) {
		buf[n++] = *in++;
	}
	buf[n] = '\0';

	char *p = buf;

	while (*p != '\0') {
		while (*p == '/') {
			p++;
		}

		if (*p == '\0') {
			break;
		}

		char *start = p;

		while (*p != '\0' && *p != '/') {
			p++;
		}

		if (*p == '/') {
			*p = '\0';
			p++;
		}

		if (strcmp(start, ".") == 0) {
			// 跳过
		} else if (strcmp(start, "..") == 0) {
			if (depth > 0) {
				depth--;
			}
		} else {
			if (depth < 64) {
				stack[depth++] = start;
			}
		}
	}

	int w = 0;

	if (depth == 0) {
		if (w < out_size - 1) {
			out[w++] = '/';
		}
	} else {
		for (int i = 0; i < depth; i++) {
			if (w < out_size - 1) {
				out[w++] = '/';
			}

			char *s = stack[i];

			while (*s != '\0' && w < out_size - 1) {
				out[w++] = *s++;
			}
		}
	}

	out[w] = '\0';
}

void hist_load(int idx) {
	int i = 0;

	while (hist[idx][i] != '\0' && i < LINE_BUF_SIZE - 1) {
		line_buf[i] = hist[idx][i];
		i++;
	}

	line_buf[i] = '\0';
	line_len = i;
	line_cursor = i;
}

// 在历史位置编辑后，把编辑结果覆盖到 saved，pos 回最新
static void hist_edit_to_saved(void) {
	hist_pos = hist_count;

	for (int i = 0; i <= line_len; i++) {
		hist_saved[i] = line_buf[i];
	}
}

void redraw_line(void) {
	cursor_lock();
	cursor_hide();

	int prompt_len = 0;
	while (prompt_buf[prompt_len] != '\0') {
		prompt_len++;
	}

	int total = prompt_len + line_len;
	int rows = (total + total_cols - 1) / total_cols;
	if (rows == 0) {
		rows = 1;
	}

	int cursor_offset = prompt_len + line_cursor;
	int cursor_srow = line_start_row + cursor_offset / total_cols;

	if (cursor_srow >= (int)total_rows) {
		int need = cursor_srow - (int)total_rows + 1;
		scroll_screen(need);
		line_start_row -= need;
		if (line_start_row < 0) {
			line_start_row = 0;
		}
	}

	for (int i = 0; i < prompt_len; i++) {
		int offset = i;
		int row = line_start_row + offset / total_cols;
		int col = offset % total_cols;

		if (row >= (int)total_rows) {
			break;
		}

		draw_char(col, row, prompt_buf[i]);
	}

	for (int i = 0; i < line_len; i++) {
		int offset = prompt_len + i;
		int row = line_start_row + offset / total_cols;
		int col = offset % total_cols;

		if (row >= (int)total_rows) {
			break;
		}

		draw_char(col, row, line_buf[i]);
	}

	for (int i = line_len; i < line_old_len; i++) {
		int offset = prompt_len + i;
		int row = line_start_row + offset / total_cols;
		int col = offset % total_cols;

		if (row >= (int)total_rows) {
			break;
		}

		draw_char(col, row, ' ');
	}

	line_old_len = line_len;
	line_old_rows = rows;

	int offset = prompt_len + line_cursor;
	int srow = line_start_row + offset / total_cols;
	int scol = offset % total_cols;

	if (srow >= (int)total_rows) {
		srow = (int)total_rows - 1;
	}

	cursor_goto(scol, srow);
	cursor_unlock();
}

// 输出报错，格式为 来源: 报错信息[: 具体内容]
void print_error(const char *source, const char *msg, const char *detail) {
	print(source);
	print(": ");
	print(msg);

	if (detail != 0 && detail[0] != '\0') {
		print(": ");
		print(detail);
	}

	print_char('\n');
}

int check_permission(const char *source, int need, int cur) {
	if (cur >= need) {
		return 1;
	}

	print_error(source, str_msg_permission, 0);
	return 0;
}

int check_argc(const char *source, int min, int max, int cur) {
	if (cur < min) {
		print_error(source, str_msg_too_few, 0);
		return 0;
	}

	if (max != -1 && cur > max) {
		print_error(source, str_msg_too_many, 0);
		return 0;
	}

	return 1;
}

void print_cmd_not_found(const char *source, const char *cmd) {
	print_error(source, str_msg_cmd_not_found, cmd);
}

static int split_tokens(char *line, char **argv) {
	static char out[LINE_BUF_SIZE * 16];
	int argc = 0;
	int i = 0;
	int w = 0;

	while (line[i] != '\0') {
		while (line[i] == ' ') {
			i++;
		}

		if (line[i] == '\0') {
			break;
		}

		int start = w;
		int has_content = 0;
		int in_single = 0;
		int in_double = 0;

		while (line[i] != '\0') {
			if (in_single) {
				if (line[i] == '\'') {
					in_single = 0;
					i++;
				} else {
					out[w] = line[i];
					w++;
					i++;
				}
			} else if (in_double) {
				if (line[i] == '"') {
					in_double = 0;
					i++;
				} else if (line[i] == '$' && line[i + 1] == '?') {
					char buf[16];
					int n = last_exit_code;
					int k = 0;

					if (n == 0) {
						buf[k++] = '0';
					} else {
						char tmp[16];
						int t = 0;
						int neg = 0;

						if (n < 0) {
							neg = 1;
							n = -n;
						}

						while (n > 0) {
							tmp[t++] = '0' + (n % 10);
							n /= 10;
						}

						if (neg) {
							buf[k++] = '-';
						}

						while (t > 0) {
							buf[k++] = tmp[--t];
						}
					}

					buf[k] = '\0';

					for (int m = 0; buf[m] != '\0'; m++) {
						out[w] = buf[m];
						w++;
					}

					i += 2;
				} else if (line[i] == '\\' && (line[i + 1] == '"' || line[i + 1] == '\\')) {
					out[w] = line[i + 1];
					w++;
					i += 2;
				} else {
					out[w] = line[i];
					w++;
					i++;
				}
			} else {
				if (line[i] == ' ') {
					break;
				} else if (line[i] == '\\' && line[i + 1] != '\0') {
					out[w] = line[i + 1];
					w++;
					i += 2;
					has_content = 1;
				} else if (line[i] == '\'') {
					in_single = 1;
					has_content = 1;
					i++;
				} else if (line[i] == '"') {
					in_double = 1;
					has_content = 1;
					i++;
				} else if (line[i] == '$' && line[i + 1] == '?') {
					char buf[16];
					int n = last_exit_code;
					int k = 0;

					if (n == 0) {
						buf[k++] = '0';
					} else {
						char tmp[16];
						int t = 0;
						int neg = 0;

						if (n < 0) {
							neg = 1;
							n = -n;
						}

						while (n > 0) {
							tmp[t++] = '0' + (n % 10);
							n /= 10;
						}

						if (neg) {
							buf[k++] = '-';
						}

						while (t > 0) {
							buf[k++] = tmp[--t];
						}
					}

					buf[k] = '\0';

					for (int m = 0; buf[m] != '\0'; m++) {
						out[w] = buf[m];
						w++;
					}

					i += 2;
					has_content = 1;
				} else {
					out[w] = line[i];
					w++;
					i++;
					has_content = 1;
				}
			}
		}

		int was_space = (line[i] == ' ');

		if (has_content) {
			out[w] = '\0';
			w++;
			argv[argc] = &out[start];
			argc++;
		}

		if (was_space) {
			i++;
		}
	}

	argv[argc] = 0;
	return argc;
}

static char glob_buf[ARGV_MAX][GLOB_TOKEN_SIZE];
static char glob_names[256][256];

static int wildcard_match(const char *pat, const char *str) {
	while (*pat != '\0') {
		if (*pat == '*') {
			while (*pat == '*') {
				pat++;
			}

			if (*pat == '\0') {
				return 1;
			}

			while (*str != '\0') {
				if (wildcard_match(pat, str)) {
					return 1;
				}

				str++;
			}

			return wildcard_match(pat, str);
		}

		if (*str == '\0') {
			return 0;
		}

		if (*pat != *str) {
			return 0;
		}

		pat++;
		str++;
	}

	return *str == '\0';
}

static int expand_glob(int argc, char **argv, char **new_argv) {
	int new_argc = 0;

	for (int i = 0; i < argc; i++) {
		const char *tok = argv[i];

		int has_star = 0;
		for (int j = 0; tok[j] != '\0'; j++) {
			if (tok[j] == '*') {
				has_star = 1;
				break;
			}
		}

		if (!has_star) {
			if (new_argc >= ARGV_MAX) {
				break;
			}

			int n = 0;
			while (tok[n] != '\0' && n < GLOB_TOKEN_SIZE - 1) {
				glob_buf[new_argc][n] = tok[n];
				n++;
			}
			glob_buf[new_argc][n] = '\0';
			new_argv[new_argc] = glob_buf[new_argc];
			new_argc++;
			continue;
		}

		int last_slash = -1;
		for (int j = 0; tok[j] != '\0'; j++) {
			if (tok[j] == '/') {
				last_slash = j;
			}
		}

		char dir[256];
		const char *pattern;

		if (last_slash < 0) {
			int w = 0;
			while (cwd[w] != '\0' && w < 250) {
				dir[w] = cwd[w];
				w++;
			}
			if (w > 0 && dir[w - 1] != '/') {
				dir[w++] = '/';
			}
			dir[w] = '\0';
			pattern = tok;
		} else if (tok[0] == '/') {
			for (int j = 0; j <= last_slash; j++) {
				dir[j] = tok[j];
			}
			dir[last_slash + 1] = '\0';
			pattern = tok + last_slash + 1;
		} else {
			int w = 0;
			while (cwd[w] != '\0' && w < 250) {
				dir[w] = cwd[w];
				w++;
			}
			if (w > 0 && dir[w - 1] != '/') {
				dir[w++] = '/';
			}
			for (int j = 0; j <= last_slash && w < 254; j++) {
				dir[w++] = tok[j];
			}
			dir[w] = '\0';
			pattern = tok + last_slash + 1;
		}

		uint32_t dir_ino;

		if (ext2_lookup(dir, &dir_ino) != 0) {
			if (new_argc >= ARGV_MAX) {
				break;
			}

			int n = 0;
			while (tok[n] != '\0' && n < GLOB_TOKEN_SIZE - 1) {
				glob_buf[new_argc][n] = tok[n];
				n++;
			}
			glob_buf[new_argc][n] = '\0';
			new_argv[new_argc] = glob_buf[new_argc];
			new_argc++;
			continue;
		}

		int before = new_argc;
		int n = ext2_list_names(dir_ino, glob_names, 256);

		for (int k = 0; k < n; k++) {
			if (!wildcard_match(pattern, glob_names[k])) {
				continue;
			}

			if (new_argc >= ARGV_MAX) {
				break;
			}

			int w = 0;
			for (int j = 0; dir[j] != '\0' && w < GLOB_TOKEN_SIZE - 1; j++) {
				glob_buf[new_argc][w] = dir[j];
				w++;
			}
			for (int j = 0; glob_names[k][j] != '\0' && w < GLOB_TOKEN_SIZE - 1; j++) {
				glob_buf[new_argc][w] = glob_names[k][j];
				w++;
			}
			glob_buf[new_argc][w] = '\0';
			new_argv[new_argc] = glob_buf[new_argc];
			new_argc++;
		}

		if (new_argc == before) {
			if (new_argc >= ARGV_MAX) {
				break;
			}

			int m = 0;
			while (tok[m] != '\0' && m < GLOB_TOKEN_SIZE - 1) {
				glob_buf[new_argc][m] = tok[m];
				m++;
			}
			glob_buf[new_argc][m] = '\0';
			new_argv[new_argc] = glob_buf[new_argc];
			new_argc++;
		}
	}

	new_argv[new_argc] = 0;
	return new_argc;
}

void handle_command(char *line) {
	static char *argv[ARGV_MAX];
	static char *expanded_argv[ARGV_MAX];
	int argc = split_tokens(line, argv);

	if (argc == 0) {
		return;
	}

	int eargc = expand_glob(argc, argv, expanded_argv);

	for (int i = 0; i < command_count; i++) {
		if (strcmp(expanded_argv[0], commands[i].name) == 0) {
			last_exit_code = commands[i].func(eargc - 1, expanded_argv + 1, current_privilege);
			return;
		}
	}

	print_cmd_not_found("dogus", expanded_argv[0]);
	last_exit_code = 127;
}

void shell_init(void) {
	build_prompt();

	line_start_row = 0;
	line_old_rows = 0;

	cursor_goto(0, 0);

	line_len = 0;
	line_cursor = 0;
	line_buf[0] = '\0';
	redraw_line();
}

void shell_run(void) {
	while (1) {
		__asm__ volatile ("hlt");

		if ((int64_t)(ticks - cursor_blink_next) >= 0) {
			cursor_visible = !cursor_visible;

			cursor_blink_next = ticks + CURSOR_BLINK_MS;

			cursor_hide();
			cursor_show();
		}

		char c = get_key();
		if (c == 0) {
			continue;
		}

		if (c == '\n') {
			int prompt_len = 0;
			while (prompt_buf[prompt_len] != '\0') {
				prompt_len++;
			}

			int total = prompt_len + line_len;
			int rows = (total + total_cols - 1) / total_cols;
			if (rows == 0) {
				rows = 1;
			}

			int next_row = line_start_row + rows;

			if (next_row >= (int)total_rows) {
				int need = next_row - (int)total_rows + 1;
				scroll_screen(need);
				next_row -= need;
				line_start_row -= need;
				if (line_start_row < 0) {
					line_start_row = 0;
				}
			}

			if (line_len > 0) {
				int same = 0;

				if (hist_count > 0) {
					same = 1;

					for (int i = 0; i < LINE_BUF_SIZE; i++) {
						if (hist[hist_count - 1][i] != line_buf[i]) {
							same = 0;
							break;
						}

						if (line_buf[i] == '\0') {
							break;
						}
					}
				}

				if (!same) {
					if (hist_count == HIST_SIZE) {
						for (int i = 0; i < HIST_SIZE - 1; i++) {
							for (int j = 0; j < HIST_LINE_LEN; j++) {
								hist[i][j] = hist[i + 1][j];
							}
						}

						hist_count--;
					}

					for (int i = 0; i <= line_len; i++) {
						hist[hist_count][i] = line_buf[i];
					}

					hist_count++;
				}
			}

			hist_pos = hist_count;

			line_buf[line_len] = '\0';
			cursor_move(-cursor_col, 1);
			handle_command(line_buf);

			int cmd_end_row = cursor_row;
			if (cursor_col != 0) {
				cmd_end_row++;
			}

			if (cmd_end_row >= (int)total_rows) {
				int need = cmd_end_row - (int)total_rows + 1;
				scroll_screen(need);
				cmd_end_row -= need;
				line_start_row -= need;
				if (line_start_row < 0) {
					line_start_row = 0;
				}
			}

			line_len = 0;
			line_cursor = 0;
			line_buf[0] = '\0';
			line_start_row = cmd_end_row;
			line_old_rows = 0;
			line_old_len = 0;

			redraw_line();
		} else if (c == '\b') {
			if (line_cursor > 0) {
				for (int i = line_cursor - 1; i < line_len - 1; i++) {
					line_buf[i] = line_buf[i + 1];
				}

				line_len--;
				line_cursor--;
				line_buf[line_len] = '\0';

				if (hist_pos != hist_count) {
					hist_edit_to_saved();
				}

				redraw_line();
			}
		} else if (c == KEY_DELETE) {
			if (line_cursor < line_len) {
				for (int i = line_cursor; i < line_len - 1; i++) {
					line_buf[i] = line_buf[i + 1];
				}

				line_len--;
				line_buf[line_len] = '\0';

				if (hist_pos != hist_count) {
					hist_edit_to_saved();
				}

				redraw_line();
			}
		} else if (c == KEY_LEFT) {
			if (line_cursor > 0) {
				line_cursor--;
				redraw_line();
			}
		} else if (c == KEY_RIGHT) {
			if (line_cursor < line_len) {
				line_cursor++;
				redraw_line();
			}
		} else if (c == KEY_HOME) {
			line_cursor = 0;
			redraw_line();
		} else if (c == KEY_END) {
			line_cursor = line_len;
			redraw_line();
		} else if (c == KEY_UP) {
			if (hist_pos > 0) {
				if (hist_pos == hist_count) {
					for (int i = 0; i <= line_len; i++) {
						hist_saved[i] = line_buf[i];
					}
				}

				hist_pos--;
				hist_load(hist_pos);
				redraw_line();
			}
		} else if (c == KEY_DOWN) {
			if (hist_pos < hist_count) {
				hist_pos++;

				if (hist_pos == hist_count) {
					for (int i = 0; i < LINE_BUF_SIZE; i++) {
						line_buf[i] = hist_saved[i];
					}

					line_len = 0;
					while (line_buf[line_len] != '\0') {
						line_len++;
					}

					line_cursor = line_len;
				} else {
					hist_load(hist_pos);
				}

				redraw_line();
			}
		} else {
			if (line_len < LINE_BUF_SIZE - 1) {
				for (int i = line_len; i > line_cursor; i--) {
					line_buf[i] = line_buf[i - 1];
				}

				line_buf[line_cursor] = c;
				line_len++;
				line_cursor++;
				line_buf[line_len] = '\0';

				if (hist_pos != hist_count) {
					hist_edit_to_saved();
				}

				redraw_line();
			}
		}
	}
}