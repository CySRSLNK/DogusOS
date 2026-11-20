#ifndef SHELL_H
#define SHELL_H

#define HIST_SIZE	64
#define HIST_LINE_LEN	256

extern uint32_t current_uid;
extern uint32_t current_euid;
extern uint32_t current_egid;

extern char current_user[32];
extern char current_home[256];

extern int last_exit_code;

#define HOST_NAME_MAX 64
extern char host[HOST_NAME_MAX];

extern char cwd[256];
extern char path_buf[256];
extern char oldpwd[256];

extern char hist[HIST_SIZE][HIST_LINE_LEN];
extern int hist_count;

void shell_init(void);
void shell_run(void);

void build_prompt(void);
void path_normalize(const char *in, char *out, int out_size);
void print_cmd_not_found(const char *source, const char *cmd);
void print_error(const char *source, const char *msg, const char *detail);

int read_password(char *buf, int max);
int check_password(const char *source, const char *name);
void handle_cursor_blink(void);
int check_argc(const char *source, int min, int max, int cur);

#endif