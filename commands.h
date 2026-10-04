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

int cmd_cd(int argc, char **argv, int privilege);
int cmd_clear(int argc, char **argv, int privilege);
int cmd_echo(int argc, char **argv, int privilege);
int cmd_false(int argc, char **argv, int privilege);
int cmd_help(int argc, char **argv, int privilege);
int cmd_history(int argc, char **argv, int privilege);
int cmd_hostname(int argc, char **argv, int privilege);
int cmd_ls(int argc, char **argv, int privilege);
int cmd_reboot(int argc, char **argv, int privilege);
int cmd_true(int argc, char **argv, int privilege);
int cmd_uptime(int argc, char **argv, int privilege);
int cmd_whoami(int argc, char **argv, int privilege);

#endif