#ifndef SHELL_H
#define SHELL_H

#define HIST_SIZE	64
#define HIST_LINE_LEN	256

extern int current_privilege;
extern int last_exit_code;

extern const char *user;
extern const char *host;

extern char hist[HIST_SIZE][HIST_LINE_LEN];
extern int hist_count;

void shell_init(void);
void shell_run(void);
void print_cmd_not_found(const char *source, const char *cmd);

#endif