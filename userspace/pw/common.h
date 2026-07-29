#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <limits.h>
#include <pwd.h>

extern int badname;
extern const char *comment;
extern mode_t home_mode;
extern const char *home;
extern const char *expire_date;
extern const char *inactive;
extern gid_t gid;
extern char **groups;
extern const char *class;
extern int create_home;
extern int non_unique;
extern const char *password;
extern const char *root;
extern const char *prefix;
extern const char *shell;
extern uid_t uid;
extern const char *program_name;
extern const char *name;
extern char **_argv;
extern uid_t min_uid;
extern uid_t max_uid;

typedef struct bitmap bitmap_t;
bitmap_t *bitmap_create(size_t size);
void bitmap_set(bitmap_t *bitmap, long bit);
long bitmap_allocate(bitmap_t *bitmap);

int useradd(int argc, char **argv);
int usermod(int argc, char **argv);
int userdel(int argc, char **argv);
void load_conf(void);
void error(const char *fmt, ...);
struct passwd *get_pwd(void);
void put_pwd(struct passwd *pwd);
void init_pwd(void);
void finish_pwd(void);
void common_setup(void);
void check_name(const char *name);
void common_arg(int opt);
#endif
