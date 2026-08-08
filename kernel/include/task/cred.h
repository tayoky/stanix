#ifndef KERNEL_CRED_H
#define KERNEL_CRED_H

#include <refcount.h>

typedef struct cred {
	ref_count_t ref_count;
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
} cred_t;

void init_cred(void);

void cred_release(cred_t *cred);
cred_t *cred_dup(cred_t *cred);

static inline cred_t *cred_ref(cred_t *cred) {
	if (cred) ref_count_inc(&cred->ref_count);
	return cred;
}

extern cred_t default_cred;

#endif
