#include <kernel/limine.h>
#include <kernel/bootinfo.h>
#include <kernel/acpi.h>
#include <kernel/kernel.h>
#include <kernel/cmdline.h>
#include <kernel/mmu.h>
#include <kernel/print.h>

__attribute__((used, section(".limine_requests_start"))) LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests"))) volatile struct limine_kernel_address_request kernel_address_request = {
	.id = LIMINE_KERNEL_ADDRESS_REQUEST,
	.revision = 2
};

__attribute__((used, section(".limine_requests"))) volatile struct limine_memmap_request memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0
};

__attribute__((used, section(".limine_requests"))) volatile struct limine_hhdm_request hhdm_request = {
	.id = LIMINE_HHDM_REQUEST
};

__attribute__((used, section(".limine_requests"))) volatile struct limine_rsdp_request rsdp_request = {
	.id = LIMINE_RSDP_REQUEST,
};

__attribute__((used, section(".limine_requests"))) volatile struct limine_executable_cmdline_request cmdline_request = {
	.id = LIMINE_EXECUTABLE_CMDLINE_REQUEST,
};

__attribute__((used, section(".limine_requests"))) volatile struct limine_kernel_file_request kernel_file_request = {
	.id = LIMINE_KERNEL_FILE_REQUEST,
};

struct limine_internal_module initrd_request = {
	.flags = LIMINE_INTERNAL_MODULE_REQUIRED,
	.path = "initrd.tar",
};

struct limine_internal_module *internal_module_list[] = {
	&initrd_request,
};

__attribute__((used, section(".limine_requests"))) volatile struct limine_module_request module_request = {
	.id = LIMINE_MODULE_REQUEST,
	.revision = 0,
	.internal_modules = internal_module_list,
	.internal_module_count = 1
};
__attribute__((used, section(".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER

static const char *memmap_types[] = {
	"usable",
	"reserved",
	"acpi reclamable",
	"acpi NVS",
	"bad memory",
	"bootloader reclamable",
	"kernel and modules",
	"framebuffer"
};

static void limine_memmap_get_entry(size_t index, bootinfo_memmap_entry_t *entry) {
	struct limine_memmap_entry *limine_entry = memmap_request.response->entries[index];
	switch (limine_entry->type) {
	case LIMINE_MEMMAP_USABLE:
		entry->type = MEMMAP_USABLE;
		break;
	case LIMINE_MEMMAP_RESERVED:
		entry->type = MEMMAP_RESERVED;
		break;
	case LIMINE_MEMMAP_ACPI_NVS:
	case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
		entry->type = MEMMAP_ACPI;
		break;
	case LIMINE_MEMMAP_BAD_MEMORY:
		entry->type = MEMMAP_BAD;
		break;
	case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
		entry->type = MEMMAP_BOOTLOADER;
		break;
	case LIMINE_MEMMAP_KERNEL_AND_MODULES:
		entry->type = MEMMAP_KERNEL;
		break;
	case LIMINE_MEMMAP_FRAMEBUFFER:
		entry->type = MEMMAP_FRAMEBUFFER;
		break;
	}
	entry->start = limine_entry->base;
	entry->size  = limine_entry->length;
}

static bootinfo_t limine_bootinfo = {
	.name = "limine",
	.memmap_get_entry = limine_memmap_get_entry,
};

static void limine_uuid2str(char *str, struct limine_uuid *uuid) {
	size_t ptr = sprintf(str, "%08x-%04hx-%04hx-%02hhx%02hhx-", 
		uuid->a, uuid->b, uuid->c, uuid->d[0], uuid->d[1]);
	for (int i = 2; i < 8; i++) {
		ptr += sprintf(str + ptr, "%02hhx", uuid->d[i]);
	}
}

void init_limine(void) {
	kstatusf("getting limine response ...");

	// get the response from the limine request
	mmu_set_hhdm(hhdm_request.response->offset);
	limine_bootinfo.hhdm                 = hhdm_request.response->offset;
	limine_bootinfo.kernel_paddr         = kernel_address_request.response->physical_base;
	limine_bootinfo.memmap_entries_count = memmap_request.response->entry_count;
	limine_bootinfo.initrd.start         = (void*)module_request.response->modules[0]->address;
	limine_bootinfo.initrd.size          = module_request.response->modules[0]->size;

	// caculate the total amount of memory
	size_t total_memory = 0;
	for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
		int type = memmap_request.response->entries[i]->type;
		if (type == LIMINE_MEMMAP_USABLE || type == LIMINE_MEMMAP_KERNEL_AND_MODULES || type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
			total_memory += memmap_request.response->entries[i]->length;
		}
	}

	kok();
	bootinfo_set(&limine_bootinfo);

	kdebugf("info :\n");
	kdebugf("kernel loaded at Vaddress : %lx\n", kernel_address_request.response->virtual_base);
	kdebugf("                 Paddress : %lx\n", kernel_address_request.response->physical_base);
	kdebugf("memmap:\n");
	for (uint64_t i=0;i < memmap_request.response->entry_count;i++) {
		kdebugf("	segment of type %s\n", memmap_types[memmap_request.response->entries[i]->type]);
		kdebugf("		offset : %lx\n", memmap_request.response->entries[i]->base);
		kdebugf("		size   : %lu\n", memmap_request.response->entries[i]->length);
	}
	kinfof("total memory amount : %dMB\n", total_memory / (1024 * 1024));
	kdebugf("initrd loaded at 0x%lx size : %ld KB\n", limine_bootinfo.initrd.start,  limine_bootinfo.initrd.size / 1024);

	if (rsdp_request.response) {
		acpi_set_rsdp(mmu_phys2virt((uintptr_t)rsdp_request.response->address));
	}
	if (cmdline_request.response) {
		kcmdline_set(cmdline_request.response->cmdline);
	}
	if (kernel_file_request.response) {
		struct limine_file *kernel_file = kernel_file_request.response->kernel_file;
		static char disk_uuid[64];
		static char part_uuid[64];
		if (kernel_file->gpt_disk_uuid.a) {
			limine_uuid2str(disk_uuid, &kernel_file->gpt_disk_uuid);
			limine_uuid2str(part_uuid, &kernel_file->gpt_part_uuid);
		} else if (kernel_file->mbr_disk_id) {
			snprintf(disk_uuid, sizeof(disk_uuid), "%08x", kernel_file->mbr_disk_id);
			snprintf(part_uuid, sizeof(part_uuid), "%08x-%02hhx", kernel_file->mbr_disk_id, kernel_file->partition_index);
		}
		limine_bootinfo.disk_uuid = disk_uuid;
		limine_bootinfo.part_uuid = part_uuid;
	}
}