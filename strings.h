#ifndef STRINGS_H
#define STRINGS_H

// shell 报错
#define str_src_dogus "dogus"
#define str_fmt_cmd_mid ": command not found: "
#define str_err_permission_denied ": permission denied\n"
#define str_err_too_few_args ": too few arguments\n"
#define str_err_too_many_args ": too many arguments\n"

// uptime 输出
#define str_uptime_up "up "
#define str_uptime_days " days, "

// cd
#define str_cmd_cd_summary "change directory"
#define str_cmd_cd_detail "cd - change directory\n\nUsage: cd [path]\n\nChange the current working directory.\nWith no argument, change to the root directory.\n"
#define str_cd_noent "cd: No such file or directory: "
#define str_cd_notdir "cd: Not a directory: "

// ls
#define str_cmd_ls_summary "list directory"
#define str_cmd_ls_detail "ls - list directory\n\nUsage: ls [path]\n\nList the contents of the directory at the given path.\nWith no argument, list the root directory.\n"
#define str_ls_noent "ls: No such file or directory: "
#define str_ls_notdir "ls: Not a directory: "

// clear
#define str_cmd_clear_summary "clear screen"
#define str_cmd_clear_detail "clear - clear screen\n\nUsage: clear\n\nClear the screen and move the cursor to the top-left corner.\n"

// echo
#define str_cmd_echo_summary "print arguments"
#define str_cmd_echo_detail "echo - print arguments\n\nUsage: echo [string ...]\n\nPrint arguments separated by a single space, followed by a newline.\nSupports single quotes, double quotes and backslash escapes.\n"

// false
#define str_cmd_false_summary "return failure"
#define str_cmd_false_detail "false - return failure\n\nUsage: false\n\nDo nothing and return exit code 1.\n"

// help
#define str_cmd_help_summary "list commands"
#define str_cmd_help_detail "help - list commands\n\nUsage: help [command]\n\nWith no argument, list all commands with a short summary.\nWith a command name, show the detailed help of that command.\n"

// history
#define str_cmd_history_summary "show history"
#define str_cmd_history_detail "history - show history\n\nUsage: history\n\nPrint the list of previously entered commands, one per line, prefixed by a number.\n"

// hostname
#define str_cmd_hostname_summary "print host name"
#define str_cmd_hostname_detail "hostname - print host name\n\nUsage: hostname\n\nPrint the host name of this system.\n"

// ls
#define str_cmd_ls_summary "list directory"
#define str_cmd_ls_detail "ls - list directory\n\nUsage: ls [path]\n\nList the contents of the directory at the given path.\nWith no argument, list the root directory.\n"
#define str_ls_noent "ls: No such file or directory: "
#define str_ls_notdir "ls: Not a directory: "

// reboot
#define str_cmd_reboot_summary "reboot system"
#define str_cmd_reboot_detail "reboot - reboot system\n\nUsage: reboot\n\nReboot the system. On QEMU this resets the virtual machine.\n"

// true
#define str_cmd_true_summary "return success"
#define str_cmd_true_detail "true - return success\n\nUsage: true\n\nDo nothing and return exit code 0.\n"

// uptime
#define str_cmd_uptime_summary "show uptime"
#define str_cmd_uptime_detail "uptime - show uptime\n\nUsage: uptime\n\nPrint how long the system has been running, in the form: up N days, H:MM:SS.\n"

// whoami
#define str_cmd_whoami_summary "print user name"
#define str_cmd_whoami_detail "whoami - print user name\n\nUsage: whoami\n\nPrint the name of the current user.\n"

#endif