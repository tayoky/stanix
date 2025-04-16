#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/vfs.h>
#include <kernel/kernel.h>
#include <kernel/paging.h>
#include <module/pci.h>
#include <errno.h>

int nvme_count = 0;

typedef struct {
	uint64_t cap;
	uint32_t version;
	uint32_t interrupt_mask_set;
	uint32_t interrupt_mask_clear;
	uint32_t controller_configuration;
	uint32_t status;
	uint32_t admin_queue_attr;
	uint32_t admin_submission_queue;
	uint32_t admin_completion_queue;
} nvme_regs;

static void init_nvme(uint64_t BAR){
	volatile nvme_regs *nvme = (nvme_regs *)(BAR + kernel->hhdm);
	
}

static void check_dev(uint8_t bus,uint8_t device,uint8_t function,void *arg){
	(void)arg;
	uint8_t base_class = (pci_read_config_word(bus,device,function,PCI_CONFIG_CLASS) >> 8) & 0xFF;
	uint8_t sub_class = pci_read_config_word(bus,device,function,PCI_CONFIG_CLASS) & 0xFF;

	if((base_class == 1) && (sub_class == 8)){
		kdebugf("nvme : find nvme disk on %d:%d:%d\n",bus,device,function);
		nvme_count++;
	}
}

static int init_nvme(int argc, char **argv){
	(void)argc;
	(void)argv;

	pci_foreach(check_dev,NULL);
	if(!nvme_count){
		kdebugf("nvme : no nvme disk found");
		//return an error
		// so the module loader remove us
		return -ENODEV;
	}
	return 0;
}

static int fini_nvme(){
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_nvme,
	.fini = fini_nvme,
	.author = "tayoky",
	.name = "nvme",
	.description = "driver for NVM Express Controller",
	.liscence = "GPL 3"
};