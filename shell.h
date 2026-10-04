#ifndef SHELL_H
#define SHELL_H

#define HIST_SIZE	64
#define HIST_LINE_LEN	256

extern int current_privilege;
extern int last_exit_code;

extern const char *user;
extern const char *host;

extern char cwd[256];
extern char path_buf[256];

extern char hist[HIST_SIZE][HIST_LINE_LEN];
extern int hist_count;

void shell_init(void);
void shell_run(void);

void build_prompt(void);
void path_normalize(const char *in, char *out, int out_size);
void print_cmd_not_found(const char *source, const char *cmd);

#endif