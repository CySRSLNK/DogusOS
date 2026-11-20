#ifndef COMMANDS_H
#define COMMANDS_H

struct opt_state {
	int index;
	int sub;
};

struct cmd {
	const char *name;
	int (*func)(int argc, char **argv);
	const char *summary;
	const char *detail;
};

extern struct cmd commands[];
extern int command_count;

int opt_next(const char *source, int argc, char **argv, const char *valid,
	     struct opt_state *st, char *ret);

int cmd_cat(int argc, char **argv);
int cmd_cd(int argc, char **argv);
int cmd_clear(int argc, char **argv);
int cmd_count(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_false(int argc, char **argv);
int cmd_help(int argc, char **argv);
int cmd_history(int argc, char **argv);
int cmd_hostname(int argc, char **argv);
int cmd_id(int argc, char **argv);
int cmd_ls(int argc, char **argv);
int cmd_mkdir(int argc, char **argv);
int cmd_passwd(int argc, char **argv);
int cmd_pwd(int argc, char **argv);
int cmd_reboot(int argc, char **argv);
int cmd_rm(int argc, char **argv);
int cmd_rmdir(int argc, char **argv);
int cmd_stat(int argc, char **argv);
int cmd_su(int argc, char **argv);
int cmd_sudo(int argc, char **argv);
int cmd_touch(int argc, char **argv);
int cmd_tree(int argc, char **argv);
int cmd_true(int argc, char **argv);
int cmd_uname(int argc, char **argv);
int cmd_uptime(int argc, char **argv);
int cmd_useradd(int argc, char **argv);
int cmd_userdel(int argc, char **argv);
int cmd_usermod(int argc, char **argv);
int cmd_users(int argc, char **argv);
int cmd_whoami(int argc, char **argv);
int cmd_write(int argc, char **argv);

#endif