#ifndef KERNEL_CRED_H
#define KERNEL_CRED_H

#include <kernel/refcount.h>
#include <abi/types.h>

typedef struct cred_groups {
	ref_count_t ref_count;
	size_t count;
	gid_t groups[];
} cred_groups_t;

typedef struct cred {
	ref_count_t ref_count;
	cred_groups_t *groups;
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
} cred_t;

void init_cred(void);

void cred_release(cred_t *cred);
cred_t *cred_new(void);
void cred_copy(cred_t *dest, cred_t *src);
cred_t *cred_dup(cred_t *cred);

static inline cred_t *cred_ref(cred_t *cred) {
	if (cred) ref_count_inc(&cred->ref_count);
	return cred;
}

void cred_groups_release(cred_groups_t *cred_groups);
cred_groups_t *cred_groups_new(size_t count);

static inline cred_groups_t *cred_groups_ref(cred_groups_t *cred_groups) {
	if (cred_groups) ref_count_inc(&cred_groups->ref_count);
	return cred_groups;
}

extern cred_t default_cred;

#endif
