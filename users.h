#ifndef USERS_H
#define USERS_H

#include <stdint.h>

#define USERS_MAX	32
#define USER_NAME_MAX	32

struct user_entry {
	char name[USER_NAME_MAX];
	uint32_t uid;
	uint32_t gid;
};

extern struct user_entry users[USERS_MAX];
extern int users_count;

int users_load(void);
int users_save(void);
int users_name_by_uid(uint32_t uid, char *name, int max);
int users_uid_by_name(const char *name, uint32_t *uid);
int users_gid_by_uid(uint32_t uid, uint32_t *gid);
int users_add(const char *name);
int users_del(const char *name);

#endif