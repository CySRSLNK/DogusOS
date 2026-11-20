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
#define str_msg_oldpwd_not_set "OLDPWD not set"
#define str_msg_option_requires_arg "option requires an argument"
#define str_msg_too_few "too few arguments"
#define str_msg_too_many "too many arguments"
#define str_msg_syntax_error "syntax error"
#define str_msg_unexpected_eol "unexpected end of line"
#define str_msg_cannot_write "cannot write"
#define str_msg_user_not_found "user not found"
#define str_msg_user_exists "user already exists"
#define str_msg_too_many_users "too many users"
#define str_msg_invalid_name "invalid name"
#define str_msg_cannot_remove_current "cannot remove current user"
#define str_msg_cannot_remove_root "cannot remove root"
#define str_msg_cannot_rename_current "cannot rename current user"
#define str_msg_cannot_create_dir "cannot create directory"
#define str_msg_incorrect_password "incorrect password"
#define str_msg_password_mismatch "passwords do not match"
#define str_msg_no_changes "no changes"

// uptime 输出
#define str_uptime_up "up "
#define str_uptime_days " days, "

// 密码提示
#define str_password_prompt "Password: "

// 登录与首次启动
#define str_login_suffix " login: "
#define str_login_incorrect "Login incorrect\n"
#define str_first_boot_header "First boot, setting root password.\n\n"
#define str_new_password_prompt "New password: "
#define str_retype_password_prompt "Retype new password: "
#define str_password_mismatch_retry "Passwords do not match, try again.\n"
#define str_root_password_set "Root password set.\n"

// cat
#define str_cmd_cat_summary "concatenate files"
#define str_cmd_cat_detail "cat - concatenate files\n\nUsage: cat file ...\n\nPrint the contents of the given files, one after another.\nNo separator or trailing newline is added.\n"

// cd
#define str_cmd_cd_summary "change directory"
#define str_cmd_cd_detail "cd - change directory\n\nUsage: cd [path]\n\nChange the current working directory.\nWith no argument, change to the home directory.\nWith -, change to the previous directory and print it.\nPaths may be absolute or relative, and may contain . and ..\n"

// clear
#define str_cmd_clear_summary "clear screen"
#define str_cmd_clear_detail "clear - clear screen\n\nUsage: clear\n\nClear the screen and move the cursor to the top-left corner.\n"

// count
#define str_cmd_count_summary "show task counters"
#define str_cmd_count_detail "count - show task counters\n\nUsage: count\n\nPrint the iteration counters of the two background kernel tasks.\nIf both keep growing across invocations, preemptive scheduling is working.\n"

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

// id
#define str_cmd_id_summary "print user identity"
#define str_cmd_id_detail "id - print user identity\n\nUsage: id\n\nPrint the current user name, UID and GID.\n"

// ls
#define str_cmd_ls_summary "list directory"
#define str_cmd_ls_detail "ls - list directory\n\nUsage: ls [-a] [-i] [-l] [path]\n\nList the contents of the directory at the given path.\nWith no path, list the current directory.\nIf path is a file, list just that file.\n-a  show hidden entries\n-i  show inode numbers\n-l  use long listing format (mode, uid, gid, size, name)\nOptions may be combined, for example -ail.\nOutput is multi-column except with -l.\n"

// mkdir
#define str_cmd_mkdir_summary "create directories"
#define str_cmd_mkdir_detail "mkdir - create directories\n\nUsage: mkdir [-p] dir ...\n\nCreate one or more directories.\n-p  make parent directories as needed\nWithout -p, the parent directory must exist and be writable.\n"

// passwd
#define str_cmd_passwd_summary "change password"
#define str_cmd_passwd_detail "passwd - change password\n\nUsage: passwd [user]\n\nWith no argument, change the password of the current user.\nWith a user name, change that user's password. Requires root.\nThe new password is requested twice and is not echoed.\n"

// pwd
#define str_cmd_pwd_summary "print working directory"
#define str_cmd_pwd_detail "pwd - print working directory\n\nUsage: pwd\n\nPrint the current working directory.\n"

// reboot
#define str_cmd_reboot_summary "reboot system"
#define str_cmd_reboot_detail "reboot - reboot system\n\nUsage: reboot\n\nReboot the system. Requires root.\n"

// rm
#define str_cmd_rm_summary "remove files"
#define str_cmd_rm_detail "rm - remove files\n\nUsage: rm [-r] [-f] file ...\n\nRemove files.\n-r  remove directories recursively\n-f  ignore nonexistent files\nOptions may be combined, for example -rf.\nWhether a file may be removed depends on the write permission of its parent directory.\n"

// rmdir
#define str_cmd_rmdir_summary "remove empty directories"
#define str_cmd_rmdir_detail "rmdir - remove empty directories\n\nUsage: rmdir dir ...\n\nRemove one or more empty directories.\nThe parent directory must be writable.\n"

// stat
#define str_cmd_stat_summary "show file status"
#define str_cmd_stat_detail "stat - show file status\n\nUsage: stat file\n\nPrint inode number, size, blocks, links, mode, uid and gid.\n"

// su
#define str_cmd_su_summary "switch user"
#define str_cmd_su_detail "su - switch user\n\nUsage: su [-] [-l] [user]\n\nSwitch to the named user.\nWith no argument, switch to root.\nWith - or -l, also change to the target user's home directory.\nIf the target user has a password and the current user is not root, it is requested.\n"

// sudo
#define str_cmd_sudo_summary "execute as superuser"
#define str_cmd_sudo_detail "sudo - execute as superuser\n\nUsage: sudo command [args ...]\n\nExecute the given command as root.\nIf the current user has a password, it is requested.\n"

// touch
#define str_cmd_touch_summary "create empty file"
#define str_cmd_touch_detail "touch - create empty file\n\nUsage: touch file ...\n\nCreate an empty file if it does not exist.\nIf the file already exists, update its access and modification times.\n"

// tree
#define str_cmd_tree_summary "list directory tree"
#define str_cmd_tree_detail "tree - list directory tree\n\nUsage: tree [path]\n\nList the contents of the directory in a tree-like format.\nHidden entries are not shown.\nRecursion depth is limited to 8.\n"

// true
#define str_cmd_true_summary "return success"
#define str_cmd_true_detail "true - return success\n\nUsage: true\n\nDo nothing and return exit code 0.\nThe last exit code is available as $?\n"

// uname
#define str_cmd_uname_summary "print system information"
#define str_cmd_uname_detail "uname - print system information\n\nUsage: uname [-a] [-s] [-n] [-r] [-v] [-m] [-o]\n\nPrint system information.\nWith no option, print the kernel name, same as -s.\n-a  print all of the following\n-s  kernel name, with any trailing OS removed\n-n  host name\n-r  kernel release\n-v  kernel build date and time\n-m  machine hardware name\n-o  operating system\nOptions may be combined, for example -sn.\n"

// uptime
#define str_cmd_uptime_summary "show uptime"
#define str_cmd_uptime_detail "uptime - show uptime\n\nUsage: uptime\n\nPrint how long the system has been running, in the form: up N days, H:MM:SS.\n"

// useradd
#define str_cmd_useradd_summary "create a new user"
#define str_cmd_useradd_detail "useradd - create a new user\n\nUsage: useradd name\n\nCreate a new user with the given name.\nAn unused UID is assigned automatically.\nThe home directory is created.\nRequires root.\n"

// userdel
#define str_cmd_userdel_summary "delete a user"
#define str_cmd_userdel_detail "userdel - delete a user\n\nUsage: userdel name\n\nDelete the named user from /etc/passwd and /etc/shadow.\nroot and the current user cannot be removed.\nRequires root.\n"

// usermod
#define str_cmd_usermod_summary "modify a user account"
#define str_cmd_usermod_detail "usermod - modify a user account\n\nUsage: usermod -l new_name old_name\n\n-l  rename the login name of old_name to new_name\nRequires root.\nThe current logged-in user cannot be renamed.\nWith no modification option, nothing is changed and exit status is 0.\n"

// users
#define str_cmd_users_summary "list users"
#define str_cmd_users_detail "users - list users\n\nUsage: users\n\nPrint the name of every user in /etc/passwd, one per line.\n"

// whoami
#define str_cmd_whoami_summary "print user name"
#define str_cmd_whoami_detail "whoami - print user name\n\nUsage: whoami\n\nPrint the name of the current user.\n"

// write
#define str_cmd_write_summary "write to file"
#define str_cmd_write_detail "write - write to file\n\nUsage: write [-a] [-n] file [string ...]\n\nWrite arguments to the given file, separated by spaces.\n-a  append instead of overwrite\n-n  do not write the trailing newline\nOptions may be combined, for example -an.\nArguments may also be redirected with > and >>.\n"

#endif