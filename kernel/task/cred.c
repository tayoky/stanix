#include <kernel/cred.h>
#include <kernel/slab.h>
#include <kernel/process.h>

static slab_cache_t creds_slab;

static cred_groups_t default_cred_groups = {
	.ref_count = 1, 
	.count = 0
};
cred_t default_cred = {
	.ref_count = 0,
	.groups = &default_cred_groups,
};

void init_cred(void) {
	slab_init(&creds_slab, sizeof(cred_t), "creds");
}

void cred_release(cred_t *cred) {
	if (!cred) return;
	if (ref_count_dec(&cred->ref_count) > 1) {
		return;
	}
	cred_groups_release(cred->groups);
	slab_free(cred);
}

cred_t *cred_new(void) {
	return slab_alloc(&creds_slab);
}

void cred_copy(cred_t *dest, cred_t *src) {
	dest->ref_count = 1;
	dest->groups    = cred_groups_ref(src->groups);
	dest->uid  = src->uid;
	dest->euid = src->euid;
	dest->suid = src->suid;
	dest->gid  = src->gid;
	dest->egid = src->egid;
	dest->sgid = src->sgid;
}

cred_t *cred_dup(cred_t *cred) {
	cred_t *new_cred = cred_new();
	if (!new_cred) return NULL;
	cred_copy(new_cred, cred);
	return new_cred;
}

void cred_groups_release(cred_groups_t *cred_groups) {
	if (!cred_groups) return;
	if (ref_count_dec(&cred_groups->ref_count) > 1) {
		return;
	}
	kfree(cred_groups);
}

cred_groups_t *cred_groups_new(size_t count) {
	cred_groups_t *cred_groups = kmalloc(sizeof(cred_groups_t) + count * sizeof(gid_t));
	if (!cred_groups) return NULL;
	cred_groups->ref_count = 1;
	cred_groups->count     = count;
	return cred_groups;
}
