#ifndef USERS_H
#define USERS_H

#include <stdint.h>

#define USERS_MAX	32
#define USER_NAME_MAX	32
#define USER_HOME_MAX	256
#define USER_PASS_MAX	64

struct user_entry {
	char name[USER_NAME_MAX];
	uint32_t uid;
	uint32_t gid;
	char home[USER_HOME_MAX];
	char password[USER_PASS_MAX];
};

extern struct user_entry users[USERS_MAX];
extern int users_count;

int users_load(void);
int users_save(void);
int users_name_by_uid(uint32_t uid, char *name, int max);
int users_uid_by_name(const char *name, uint32_t *uid);
int users_gid_by_uid(uint32_t uid, uint32_t *gid);
int users_home_by_uid(uint32_t uid, char *home, int max);
int users_password_by_name(const char *name, char *password, int max);
int users_add(const char *name);
int users_del(const char *name);
int users_rename(const char *old_name, const char *new_name);

#endif