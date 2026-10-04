#include <stdint.h>
#include "kernel.h"
#include "shell.h"
#include "commands.h"
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
const char *path = "~";

char prompt_buf[128] = { 0 };

char hist[HIST_SIZE][HIST_LINE_LEN];
int hist_count = 0;
int hist_pos = 0;
char hist_saved[LINE_BUF_SIZE];

int current_privilege = 0;
int last_exit_code = 0;

#define ARGV_MAX	(LINE_BUF_SIZE / 2 + 1)

void build_prompt(void) {
	int i = 0;

	while (user[i] != '\0') {
		prompt_buf[i] = user[i];
		i++;
	}

	prompt_buf[i++] = '@';

	int j = 0;
	while (host[j] != '\0') {
		prompt_buf[i++] = host[j];
		j++;
	}

	prompt_buf[i++] = ':';

	j = 0;
	while (path[j] != '\0') {
		prompt_buf[i++] = path[j];
		j++;
	}

	prompt_buf[i++] = '$';
	prompt_buf[i++] = ' ';
	prompt_buf[i] = '\0';
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

int check_permission(int need, int cur) {
	if (cur >= need) {
		return 1;
	}

	print(str_src_dogus);
	print(str_err_permission_denied);
	return 0;
}

int check_argc(int min, int max, int cur) {
	if (cur < min) {
		print(str_src_dogus);
		print(str_err_too_few_args);
		return 0;
	}

	if (max != -1 && cur > max) {
		print(str_src_dogus);
		print(str_err_too_many_args);
		return 0;
	}

	return 1;
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

void print_cmd_not_found(const char *source, const char *cmd) {
	print(source);
	print(str_fmt_cmd_mid);
	print(cmd);
	print_char('\n');
}

void handle_command(char *line) {
	char *argv[ARGV_MAX];
	int argc = split_tokens(line, argv);

	if (argc == 0) {
		return;
	}

	for (int i = 0; i < command_count; i++) {
		if (strcmp(argv[0], commands[i].name) == 0) {
			last_exit_code = commands[i].func(argc - 1, argv + 1, current_privilege);
			return;
		}
	}

	print_cmd_not_found(str_src_dogus, argv[0]);
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
					for (int i = 0; i <= line_len; i++) {
						hist[hist_pos][i] = line_buf[i];
					}
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
					for (int i = 0; i <= line_len; i++) {
						hist[hist_pos][i] = line_buf[i];
					}
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
					for (int i = 0; i <= line_len; i++) {
						hist[hist_pos][i] = line_buf[i];
					}
				}

				redraw_line();
			}
		}
	}
}