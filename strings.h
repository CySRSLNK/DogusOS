#ifndef STRINGS_H
#define STRINGS_H

// 通用报错信息
#define str_msg_cmd_not_found "command not found"
#define str_msg_noent "No such file or directory"
#define str_msg_notdir "Not a directory"
#define str_msg_isdir "Is a directory"
#define str_msg_exists "File exists"
#define str_msg_notempty "Directory not empty"
#define str_msg_nospace "No space left on device"
#define str_msg_permission "Permission denied"
#define str_msg_invalid_option "invalid option"
#define str_msg_too_few "too few arguments"
#define str_msg_too_many "too many arguments"

// uptime 输出
#define str_uptime_up "up "
#define str_uptime_days " days, "

// cat
#define str_cmd_cat_summary "concatenate files"
#define str_cmd_cat_detail "cat - concatenate files\n\nUsage: cat file ...\n\nPrint the contents of the given files, one after another.\nNo separator or trailing newline is added.\n"

// cd
#define str_cmd_cd_summary "change directory"
#define str_cmd_cd_detail "cd - change directory\n\nUsage: cd [path]\n\nChange the current working directory.\nWith no argument, change to the root directory.\nPaths may be absolute or relative, and may contain . and ..\n"

// clear
#define str_cmd_clear_summary "clear screen"
#define str_cmd_clear_detail "clear - clear screen\n\nUsage: clear\n\nClear the screen and move the cursor to the top-left corner.\n"

// echo
#define str_cmd_echo_summary "print arguments"
#define str_cmd_echo_detail "echo - print arguments\n\nUsage: echo [-n] [string ...]\n\nPrint arguments separated by a single space.\nWith -n, do not print the trailing newline.\nSupports single quotes, double quotes and backslash escapes.\nArguments may be redirected with > and >>.\n"

// false
#define str_cmd_false_summary "return failure"
#define str_cmd_false_detail "false - return failure\n\nUsage: false\n\nDo nothing and return exit code 1.\nThe last exit code is available as $?\n"

// help
#define str_cmd_help_summary "list commands"
#define str_cmd_help_detail "help - list commands\n\nUsage: help [command]\n\nWith no argument, list all commands with a short summary.\nWith a command name, show the detailed help of that command.\n"

// history
#define str_cmd_history_summary "show history"
#define str_cmd_history_detail "history - show history\n\nUsage: history\n\nPrint the list of previously entered commands, one per line, prefixed by a number.\nHistory stores up to 64 entries, skips empty lines, and removes adjacent duplicates.\n"

// hostname
#define str_cmd_hostname_summary "print host name"
#define str_cmd_hostname_detail "hostname - print host name\n\nUsage: hostname\n\nPrint the host name of this system.\n"

// ls
#define str_cmd_ls_summary "list directory"
#define str_cmd_ls_detail "ls - list directory\n\nUsage: ls [-a] [-i] [-l] [path]\n\nList the contents of the directory at the given path.\nWith no path, list the current directory.\n-a  show hidden entries\n-i  show inode numbers\n-l  use long listing format (type, size, name)\nOptions may be combined, for example -ail.\nOutput is multi-column except with -l.\n"

// mkdir
#define str_cmd_mkdir_summary "create directories"
#define str_cmd_mkdir_detail "mkdir - create directories\n\nUsage: mkdir dir ...\n\nCreate one or more directories.\nThe parent directory must exist.\n"

// pwd
#define str_cmd_pwd_summary "print working directory"
#define str_cmd_pwd_detail "pwd - print working directory\n\nUsage: pwd\n\nPrint the current working directory.\n"

// reboot
#define str_cmd_reboot_summary "reboot system"
#define str_cmd_reboot_detail "reboot - reboot system\n\nUsage: reboot\n\nReboot the system. On QEMU this resets the virtual machine.\n"

// rm
#define str_cmd_rm_summary "remove files"
#define str_cmd_rm_detail "rm - remove files\n\nUsage: rm [-r] [-f] file ...\n\nRemove files.\n-r  remove directories recursively\n-f  ignore nonexistent files\nOptions may be combined, for example -rf.\nA file with a name starting with a dash may be reached as ./-name\n"

// rmdir
#define str_cmd_rmdir_summary "remove empty directories"
#define str_cmd_rmdir_detail "rmdir - remove empty directories\n\nUsage: rmdir dir ...\n\nRemove one or more empty directories.\nA directory is not removed if it contains any entry other than . and ..\n"

// stat
#define str_cmd_stat_summary "show file status"
#define str_cmd_stat_detail "stat - show file status\n\nUsage: stat file\n\nPrint inode number, size, blocks, links and file type.\n"

// sudo
#define str_cmd_sudo_summary "execute as superuser"
#define str_cmd_sudo_detail "sudo - execute as superuser\n\nUsage: sudo command [args ...]\n\nExecute the given command with elevated privilege.\nWith no command, do nothing.\nNested sudo is allowed.\n"

// touch
#define str_cmd_touch_summary "create empty file"
#define str_cmd_touch_detail "touch - create empty file\n\nUsage: touch file ...\n\nCreate an empty file if it does not exist.\nIf the file already exists, do nothing.\n"

// tree
#define str_cmd_tree_summary "list directory tree"
#define str_cmd_tree_detail "tree - list directory tree\n\nUsage: tree [path]\n\nList the contents of the directory in a tree-like format.\nHidden entries are not shown.\nRecursion depth is limited to 8.\n"

// true
#define str_cmd_true_summary "return success"
#define str_cmd_true_detail "true - return success\n\nUsage: true\n\nDo nothing and return exit code 0.\nThe last exit code is available as $?\n"

// uptime
#define str_cmd_uptime_summary "show uptime"
#define str_cmd_uptime_detail "uptime - show uptime\n\nUsage: uptime\n\nPrint how long the system has been running, in the form: up N days, H:MM:SS.\n"

// whoami
#define str_cmd_whoami_summary "print user name"
#define str_cmd_whoami_detail "whoami - print user name\n\nUsage: whoami\n\nPrint the name of the current user.\n"

// write
#define str_cmd_write_summary "write to file"
#define str_cmd_write_detail "write - write to file\n\nUsage: write [-a] [-n] file [string ...]\n\nWrite arguments to the given file, separated by spaces.\n-a  append instead of overwrite\n-n  do not write the trailing newline\nOptions may be combined, for example -an.\nArguments may also be redirected with > and >>.\n"

#endif