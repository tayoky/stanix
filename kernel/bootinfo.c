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

void get_bootinfo(kernel_table *kernel){
	kstatus("getting limine response ...");

	//get the response from the limine request
	kernel->bootinfo.kernel_address_response = kernel_address_request.response;
	kernel->bootinfo.memmap_response = memmap_request.response;
	kernel->bootinfo.boot_time_response = boot_time_request.response;

	//cacul the total amount of memory
	kernel->total_memory = kernel->bootinfo.memmap_response->entries[kernel->bootinfo.memmap_response->entry_count-1]->base;
	kernel->total_memory += kernel->bootinfo.memmap_response->entries[kernel->bootinfo.memmap_response->entry_count-1]->length;
	kok();

	kdebugf("info :\n");
	kdebugf("time at boot : %lu\n",kernel->bootinfo.boot_time_response->boot_time);
	kdebugf("kernel loaded at Vaddress : %x\n",kernel->bootinfo.kernel_address_response->virtual_base);
	kdebugf("                 Paddress : %x\n",kernel->bootinfo.kernel_address_response->physical_base);
	kdebugf("memmap:\n");
	for(uint64_t i=0;i<kernel->bootinfo.memmap_response->entry_count;i++){
		kdebugf("	segment of type %s\n",memmap_types[kernel->bootinfo.memmap_response->entries[i]->type]);
		kdebugf("		offset : %lx\n",kernel->bootinfo.memmap_response->entries[i]->base);
		kdebugf("		size   : %lu\n",kernel->bootinfo.memmap_response->entries[i]->length);
	}
	kinfof("total memory amount : %dMB\n",kernel->total_memory / (0x1000 * 0x1000));
}