#include <kernel/ioreq.h>

int ioreq_submit(ioreq_t *ioreq) {
	if (!ioreq->ops || !ioreq->ops->submit) {
		return -ENOTSUP;
	}
	// create a new ref for the submission
	ioreq_ref(ioreq);
	int ret = ioreq->ops->submit(ioreq);
	if (ret < 0) {
		ioreq_release(ioreq);
	}
	return ret;
}

int ioreq_submit_sync(ioreq_t *ioreq) {
	ioreq_ref(ioreq);
	int ret = ioreq_submit(ioreq);
	if (ret >= 0) {
		ret = ioreq_wait(ioreq);
		if (ret < 0) {
			ioreq_cancel(ioreq);
		}
	}
	ioreq_release(ioreq);
	return ret;
}

int ioreq_submit_sync_interruptible(ioreq_t *ioreq) {
	ioreq_ref(ioreq);
	int ret = ioreq_submit(ioreq);
	if (ret >= 0) {
		ret = ioreq_wait_interruptible(ioreq);
		if (ret < 0) {
			ioreq_cancel(ioreq);
		}
	}

	ioreq_release(ioreq);
	return ret;
}

void ioreq_finish(ioreq_t *ioreq, int ret) {
	ioreq->ret = ret;
	if (ioreq->callback) {
		ioreq->callback(ioreq, ioreq->data);
	}
	cond_set(&ioreq->cond, 1);
	ioreq_release(ioreq);
}
