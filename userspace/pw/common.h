#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <limits.h>
#include <pwd.h>

extern int badname;
extern const char *comment;
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

int usermod(int argc, char **argv);
int userdel(int argc, char **argv);
struct passwd *get_pwd(void);
void put_pwd(struct passwd *pwd);
void init_pwd(void);
void finish_pwd(void);
void common_setup(void);
void check_name(const char *name);
void common_arg(int opt);
#endif
