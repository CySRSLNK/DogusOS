#ifndef COMMANDS_H
#define COMMANDS_H

struct cmd {
	const char *name;
	int (*func)(int argc, char **argv, int privilege);
	const char *summary;
	const char *detail;
};

extern struct cmd commands[];
extern int command_count;

int cmd_cat(int argc, char **argv, int privilege);
int cmd_cd(int argc, char **argv, int privilege);
int cmd_clear(int argc, char **argv, int privilege);
int cmd_echo(int argc, char **argv, int privilege);
int cmd_false(int argc, char **argv, int privilege);
int cmd_help(int argc, char **argv, int privilege);
int cmd_history(int argc, char **argv, int privilege);
int cmd_hostname(int argc, char **argv, int privilege);
int cmd_id(int argc, char **argv, int privilege);
int cmd_ls(int argc, char **argv, int privilege);
int cmd_mkdir(int argc, char **argv, int privilege);
int cmd_pwd(int argc, char **argv, int privilege);
int cmd_reboot(int argc, char **argv, int privilege);
int cmd_rm(int argc, char **argv, int privilege);
int cmd_rmdir(int argc, char **argv, int privilege);
int cmd_stat(int argc, char **argv, int privilege);
int cmd_su(int argc, char **argv, int privilege);
int cmd_sudo(int argc, char **argv, int privilege);
int cmd_touch(int argc, char **argv, int privilege);
int cmd_tree(int argc, char **argv, int privilege);
int cmd_true(int argc, char **argv, int privilege);
int cmd_uptime(int argc, char **argv, int privilege);
int cmd_useradd(int argc, char **argv, int privilege);
int cmd_userdel(int argc, char **argv, int privilege);
int cmd_whoami(int argc, char **argv, int privilege);
int cmd_write(int argc, char **argv, int privilege);

#endif