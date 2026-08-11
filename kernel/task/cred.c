#include <kernel/cred.h>
#include <kernel/slab.h>
#include <kernel/process.h>

static slab_cache_t creds_slab;

cred_t default_cred = {0};

void init_cred(void) {
	slab_init(&creds_slab, sizeof(cred_t), "creds");
}

void cred_release(cred_t *cred) {
	if (!cred) return;
	if (ref_count_dec(&cred->ref_count) > 1) {
		return;
	}
	slab_free(cred);
}

cred_t *cred_dup(cred_t *cred) {
	cred_t *new_cred = slab_alloc(&creds_slab);
	if (!new_cred) return NULL;
	new_cred->ref_count = 1;
	new_cred->uid  = cred->uid;
	new_cred->euid = cred->euid;
	new_cred->suid = cred->suid;
	new_cred->gid  = cred->gid;
	new_cred->egid = cred->egid;
	new_cred->sgid = cred->sgid;
	return new_cred;
}
