#include "limine.h"
#include "bootinfo.h"
#include "kernel.h"
#include "print.h"

LIMINE_REQUESTS_START_MARKER

static volatile LIMINE_BASE_REVISION(3);

static volatile struct limine_kernel_address_request kernel_address_request = {
	.id = LIMINE_KERNEL_ADDRESS_REQUEST,
	.revision = 2
};

static volatile struct limine_memmap_request memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0
};

static volatile struct limine_boot_time_request boot_time_request = {
	.id = LIMINE_BOOT_TIME_REQUEST
};

static volatile struct limine_hhdm_request hhdm_request = {
	.id = LIMINE_HHDM_REQUEST
};

struct limine_internal_module initrd_request = {
	.path = "initrd.tar",
	.flags = LIMINE_INTERNAL_MODULE_REQUIRED
};

struct limine_internal_module internal_module_list[] ={ 
	&initrd_request
};

struct limine_module_request module_request = {
	.id = LIMINE_MODULE_REQUEST,
	.revision = 0,
	.internal_modules = &internal_module_list,
	.internal_module_count = 1
};

struct limine_framebuffer_request frambuffer_request ={
	.id = LIMINE_FRAMEBUFFER_REQUEST	
};
LIMINE_REQUESTS_END_MARKER

static const char * memmap_types[] = {
	"usable",
	"reserved",
	"acpi reclamable",
	"acpi NVS",
	"bad memory",
	"bootloader reclamable",
	"kernel and modules",
	"framebuffer"
};


void get_bootinfo(void){
	kstatus("getting limine response ...");
	//get the stack start
	uint64_t *rbp;
	asm("mov %%rbp, %%rax":"=a"(rbp));
	while (*rbp){
		rbp = (uint64_t *)*rbp;
	}
	kernel->stack_start = (uint64_t)rbp;

	//get the response from the limine request
	kernel->kernel_address = kernel_address_request.response;
	kernel->memmap = memmap_request.response;
	kernel->bootinfo.boot_time_response = boot_time_request.response;
	kernel->hhdm = hhdm_request.response->offset;
	kernel->initrd = module_request.response->modules[0];

	//cacul the total amount of memory
	kernel->total_memory = 0;
	for (uint64_t i = 0; i < kernel->memmap->entry_count; i++){
		if(kernel->memmap->entries[i]->type != LIMINE_MEMMAP_RESERVED){
			kernel->total_memory += kernel->memmap->entries[i]->length;
		}
	}
	
	kok();

	kdebugf("info :\n");
	kdebugf("stack start : 0x%lx\n",kernel->stack_start);
	kdebugf("time at boot : %lu\n",kernel->bootinfo.boot_time_response->boot_time);
	kdebugf("kernel loaded at Vaddress : %x\n",kernel->kernel_address->virtual_base);
	kdebugf("                 Paddress : %x\n",kernel->kernel_address->physical_base);
	kdebugf("memmap:\n");
	for(uint64_t i=0;i<kernel->memmap->entry_count;i++){
		kdebugf("	segment of type %s\n",memmap_types[kernel->memmap->entries[i]->type]);
		kdebugf("		offset : %lx\n",kernel->memmap->entries[i]->base);
		kdebugf("		size   : %lu\n",kernel->memmap->entries[i]->length);
	}
	kinfof("total memory amount : %dMB\n",kernel->total_memory / (1024 * 1024));
	kdebugf("initrd loaded at 0x%lx size : %ld KB\n",kernel->initrd->address,kernel->initrd->size / 1024);
}