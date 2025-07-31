#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/port.h>
#include <module/ata.h>
#include <module/pci.h>
#include <errno.h>

static size_t ata_count = 0;

typedef struct {
	int exist;
	int drive;
	int class;
	uint32_t bar;
	size_t size;
} ata_device;

#define ATA_DRIVE_MASTER 0xA0
#define ATA_DRIVE_SLAVE  0xB0

#define ATA_CLASS_ATA    0
#define ATA_CLASS_ATAPI  1

typedef struct {
	ata_device devices[4];
} ide_controller;

static void io_wait_ata(ata_device *device){
	for (size_t i = 0; i < 4; i++){in_byte(device->bar + ATA_REG_ALTSTATUS);}
}

static void reset_ata(ata_device *device){
	//soft reset
	out_byte(device->bar + ATA_REG_CONTROL,0x4);
	io_wait_ata(device);
	out_byte(device->bar + ATA_REG_CONTROL,0x0);
}

static void init_ata(ata_device *device){
	//identify
	out_byte(device->bar + ATA_REG_DRV_SELECT,device->drive);
	io_wait_ata(device);
	while(in_byte(device->bar + ATA_REG_STATUS) & ATA_SR_BSY);
	out_byte(device->bar + ATA_REG_SECCOUNT0,0);
	out_byte(device->bar + ATA_REG_LBA0,0);
	out_byte(device->bar + ATA_REG_LBA1,0);
	out_byte(device->bar + ATA_REG_LBA2,0);
	out_byte(device->bar + ATA_REG_COMMAND,ATA_CMD_IDENTIFY);
	io_wait_ata(device);
	if(in_byte(device->bar + ATA_REG_STATUS) == 0){
		//no drive
		device->exist = 0;
		return;
	}
	device->exist = 1;
	kdebugf("start polling\n");
	while(in_byte(device->bar + ATA_REG_STATUS) & ATA_SR_BSY);
	if(in_byte(device->bar + ATA_REG_LBA1) || in_byte(device->bar + ATA_REG_LBA2)){
		//atapi
		device->class = ATA_CLASS_ATAPI;
		return;
	}
	for(;;){
		uint8_t status = in_byte(device->bar + ATA_REG_STATUS);
		if(status & ATA_SR_DRQ){
			break;
		}
		if(status & ATA_SR_ERR){
			//error happend
			//TODO : maybee retry ?
			device->exist = 0;
			return;
		}
	}

	uint16_t buf[256];
	for (size_t i = 0; i < 256; i++){
		buf[i] = in_word(device->bar + ATA_REG_DATA);
	}
	uint8_t *ident = (uint8_t)buf;
	kdebugf("find usable ata drive on base %p drive %S\n",device->bar,device->drive == ATA_DRIVE_MASTER ? "master" : "slave");
}

static void check_dev(uint8_t bus,uint8_t device,uint8_t function,void *arg){
	(void)arg;
	uint8_t base_class = pci_read_config_byte(bus,device,function,PCI_CONFIG_BASE_CLASS);
	uint8_t sub_class  = pci_read_config_byte(bus,device,function,PCI_CONFIG_SUB_CLASS);
	uint8_t prog_if    = pci_read_config_byte(bus,device,function,PCI_CONFIG_PROG_IF);
	uint32_t bar       = pci_read_config_dword(bus,device,function,PCI_CONFIG_BAR4);
	

	if(!((base_class == 1) && ((sub_class == 5) || (sub_class == 1)))){
		return;
	}
	kdebugf("find ata disk on %d:%d:%d\n",bus,device,function);
	if((prog_if & 0x1) || (prog_if & 0x2)){
		kdebugf("ata disk don't support compatibility mode\n");
		return;
	}
	if(!(bar & 0x01)){
		kdebugf("bar4 isen't a io port address\n");
		return;
	}
	bar &= ~0x3;
	kdebugf("bar4 : %lx\n",bar);

	ide_controller *controller = kmalloc(sizeof(ide_controller));
	memset(controller,0,sizeof(ide_controller));

	controller->devices[0].bar = bar;
	controller->devices[1].bar = bar;
	controller->devices[2].bar = bar + 8;
	controller->devices[3].bar = bar + 8;
	controller->devices[0].drive = ATA_DRIVE_MASTER;
	controller->devices[1].drive = ATA_DRIVE_SLAVE;
	controller->devices[2].drive = ATA_DRIVE_MASTER;
	controller->devices[3].drive = ATA_DRIVE_SLAVE;
	//soft reset reset the two devices on the bus
	for (size_t i = 0; i < 4; i+=2){
		reset_ata(&controller->devices[i]);
	}
	for (size_t i = 0; i < 4; i++){
		init_ata(&controller->devices[i]);
	}
	
	
	ata_count++;
}

int ata_init(int argc,char **argv){
	(void)argc;
	(void)argv;
	pci_foreach(check_dev,NULL);
	if(!ata_count){
		kdebugf("no ata disk found\n");
		//return an error
		// so the module loader remove us
		return -ENODEV;
	}
	return 0;
}

int ata_fini(){
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = ata_init,
	.fini = ata_fini,
	.author = "tayoky",
	.name = "ata",
	.description = "ata pio driver",
	.license = "GPL 3",
};