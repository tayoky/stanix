#include <kernel/module.h>
#include <kernel/bus.h>
#include <ide.h>

int ide_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	driver_register(&ide_controller_driver);
	driver_register(&ide_channel_driver.driver);
	return 0;
}

int ide_fini(void) {
	driver_unregister(&ide_controller_driver);
	driver_unregister(&ide_channel_driver.driver);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = ide_init,
	.fini        = ide_fini,
	.author      = "tayoky",
	.name        = "IDE controller",
	.description = "IDE controller driver",
	.license     = "GPL 3",
};
