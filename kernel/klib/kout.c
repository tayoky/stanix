#include <kernel/kout.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/vfs.h>
#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/ini.h>

void init_kout(void) {
	kstatusf("init kout... ");
	const char *kout_option = kcmdline_get_option("--kout");

	static vfs_fd_t *kouts[8];
	size_t kouts_count = 0;
	char *ptr;
	char *kout = strtok_r(kout_option, ",", &ptr);
	while (kout && kouts_count + 1 < arraylen(kouts)) {
		device_t *device = device_from_name(kout);
		if (!device) {
			kwarningf("could not find device '%s'\n", kout);
			continue;
		}
		vfs_fd_t *fd = device_open(device, O_WRONLY);
		device_release(device);
		if (IS_ERR(fd)) {
			kwarningf("could not open device '%s'\n", kout);
			continue;
		}
		kouts[kouts_count++] = fd;

		kout = strtok_r(NULL, ",", &ptr);
	}
	kouts[kouts_count] = NULL;
	
	// now actually use it
	kernel->outs = kouts;

	kok();
	kinfof("found %ld kouts\n", kouts_count);
}
