#ifndef NUEVOOS_USERDB_H
#define NUEVOOS_USERDB_H

#include "types.h"

#define USERDB_MAX       8u
#define USERDB_NAME_MAX  24u
#define USERDB_PASS_MAX  32u
#define USERDB_SLUG_MAX  12u

struct user_rec {
    char name[USERDB_NAME_MAX];
    char slug[USERDB_SLUG_MAX];
    char pass[USERDB_PASS_MAX];
};

void userdb_load(void);
u32 userdb_count(void);
const struct user_rec *userdb_get(u32 index);
const struct user_rec *userdb_current(void);
u32 userdb_current_index(void);
void userdb_select(u32 index);
void userdb_select_next(void);

/* Add account, write users.db and home files. Returns 0 or -1. */
int userdb_add(const char *name, const char *pass);

/* 1 if name (display or slug) + password match. */
int userdb_auth(const char *name, const char *pass);
int userdb_auth_current(const char *pass);

#endif
