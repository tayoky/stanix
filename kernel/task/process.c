#include <kernel/process.h>
#include <kernel/slab.h>
#include <kernel/xarray.h>

static slab_cache_t process_group_slabs;
static xarray_t groups;

void init_proc(void) {
	slab_init(&process_groups_slab, sizeof(process_group_t), "process-groups");
}

process_group_t *process_group_get_from_pgid(pid_t pgid) {
	rcu_acquire_read(&groups.rcu);
	process_group_t *group = xarray_get(&groups, pgid);
	process_group_ref(group);
	rcu_release_read(&groups.rcu);
	return group;
}

process_group_t *process_group_get_or_create_from_pgid(pid_t pgid) {
	process_group_t *group = process_group_get_from_pgid(pgid);
	if (group) return group;

	// we need to create a group
	group = slab_alloc(&process_groups_slab);
	if (!group) return NULL;
	memset(group, 0, sizeof(process_group_t));
	group->ref_count = 1;

	rcu_acquire_read(&groups.rcu);
	process_group_t *race_group = xarray_cmpxchg(&groups, pgid, NULL, group);
	process_group_ref(race_group);
	rcu_release_read(&groups.rcu);
	if (race_group) {
		// we raced
		slab_free(group);
		return race_group;
	}
	return group;
}

void process_group_release(process_group_t *group) {
	if (!group) return;
	if (ref_count_dec(&group->ref) > 1) return;
	slab_free(group);
}

void proc_set_group(process_t *proc, process_group_t *group) {
	if (proc->group) {
		rculist_remove(&proc->group->processes, &proc->group_node);
		if (rculist_is_empty(&proc->group->processes)) {
			xarray_clear(&groups, &proc->group->pgid);
		}
		process_group_release(proc->group);
	}
	proc->group = process_group_ref(group);
	if (group) {
		rculist_append(&group->processes, &proc->group_node);
	}
}
