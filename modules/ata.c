#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/mutex.h>
#include <kernel/time.h>
#include <kernel/list.h>
#include <kernel/port.h>
#include <module/ata.h>
#include <module/pci.h>
#include <kernel/vfs.h>
#include <sys/ioctl.h>
#include <errno.h>

//thanks sasdallas for this
typedef struct ata_ident {
    uint16_t flags;             // If bit 15 is cleared, valid drive. If bit 7 is set to one, this is removable.
    uint16_t obsolete;          // Obsolete
    uint16_t specifics;         // 7.17.7.3 in specification
    uint16_t obsolete2[6];      // Obsolete
    uint16_t obsolete3;         // Obsolete
    char serial[20];            // Serial number
    uint16_t obsolete4[3];      // Obsolete
    char firmware[8];           // Firmware revision
    char model[40];             // Model number
    uint16_t rw_multiple;       // R/W multiple support (<=16 is SATA)
    uint16_t obsolete5;         // Obsolete
    uint32_t capabilities;      // Capabilities of the IDE device
    uint16_t obsolete6[2];      // Obsolete
    uint16_t field_validity;    // If 1, the values reported in _ - _ are valid
    uint16_t obsolete7[5];      // Obsolete
    uint16_t multi_sector;      // Multiple sector setting
    uint32_t sectors;           // Total addressible sectors
    uint16_t obsolete8[20];     // Technically these aren't obsolete, but they contain nothing really useful
    uint32_t command_sets;      // Command/feature sets
    uint16_t obsolete9[16];     // Contain nothing really useful
    uint64_t sectors_lba48;     // LBA48 maximum sectors, AND by 0000FFFFFFFFFFFF for validity
    uint16_t obsolete10[152];   // Contain nothing really useful
} __attribute__((packed)) __attribute__((aligned(8))) ata_ident;

static list *ide_controllers;
static int hdx = 'a';

typedef struct {
	mutex_t lock;
	uint8_t nIEN;
	uint32_t base;
	uint32_t ctrl;
	uint32_t bmide;
} ide_channel;

typedef struct {
	int exist;
	int drive;
	int class;
	size_t size;
	vfs_node *node;
	char model[40];
	uint32_t command_sets;
	ide_channel *channel;
} ide_device;

#define ATA_DRIVE_MASTER 0xA0
#define ATA_DRIVE_SLAVE  0xB0

#define ATA_CLASS_ATA    0
#define ATA_CLASS_ATAPI  1

typedef struct {
	ide_device devices[4];
	ide_channel channel[2];
} ide_controller;

static uint32_t reg2port(ide_device *device,uint32_t reg){
	if(reg <= ATA_REG_STATUS){
		return device->channel->base + reg;
	} else if (reg <= ATA_REG_LBA5){
		return device->channel->base + reg - 0x06;
	} else if (reg <= ATA_REG_DEVADDRESS){
		return device->channel->ctrl + reg - 0x0A;
	} else {
		return device->channel->bmide + reg - 0xE; //idk
	}
}

static void ide_write(ide_device *device,uint32_t reg,uint8_t data){
	//set HOB
	if (reg > 0x07 && reg < 0x0C){
		ide_write(device,ATA_REG_CONTROL,0x80 | device->channel->nIEN);
	}
	out_byte(reg2port(device,reg),data);

	if (reg > 0x07 && reg < 0x0C){
		ide_write(device,ATA_REG_CONTROL,device->channel->nIEN);
	}
}

static uint8_t ide_read(ide_device *device,uint32_t reg){
	if (reg > 0x07 && reg < 0x0C){
		ide_write(device,ATA_REG_CONTROL,0x80 | device->channel->nIEN);
	}
	uint8_t data = in_byte(reg2port(device,reg));
	if (reg > 0x07 && reg < 0x0C){
		ide_write(device,ATA_REG_CONTROL,device->channel->nIEN);
	}
	return data;
}

static void ide_io_wait(ide_device *device){
	for (size_t i = 0; i < 4; i++){ide_read(device,ATA_REG_ALTSTATUS);}
}

static int ide_poll(ide_device *device,uint8_t mask,uint8_t value){
	size_t timeout = 10000;
	while((ide_read(device,ATA_REG_STATUS) & mask) != value){
		if(--timeout <= 0){
			kdebugf("timeout expired\n");
			return -1;
		};
	}
	return 0;
}

static void ide_reset(ide_device *device){
	//soft reset
	ide_write(device,ATA_REG_CONTROL,0x4 | device->channel->nIEN);
	ide_io_wait(device);
	ide_write(device,ATA_REG_CONTROL,device->channel->nIEN);
}

static ssize_t ata_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	ide_device *device = node->private_inode;

	if(offset >= device->size){
		return 0;
	}
	if(offset + count >= device->size){
		count = device->size - offset;
	}

	uint64_t lba = offset / 512;
	uint64_t end = offset + count;
	size_t   sectors_count = (end + 511) / 512 - lba;
	if(!sectors_count)return 0;

	acquire_mutex(&device->channel->lock);

	//select the drive
	//TODO : don't reselect if is was already selected
	if(device->command_sets & (1 << 26)){
		//lba48
		ide_write(device,ATA_REG_DRV_SELECT,(device->drive == ATA_DRIVE_MASTER ? 0x40 : 0x50));
	} else {
		//lba28
		ide_write(device,ATA_REG_DRV_SELECT,(device->drive == ATA_DRIVE_MASTER ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
	}
	ide_io_wait(device);
	if(ide_poll(device,ATA_SR_BSY,0) < 0){
		release_mutex(&device->channel->lock);
		return -EIO;
	}

	//send lba
	if(device->command_sets & (1 << 26)){
		///lba 48
		ide_write(device,ATA_REG_SECCOUNT0,(uint8_t)(sectors_count >> 8));
		ide_write(device,ATA_REG_LBA3,(uint8_t)(lba >> 24));
		ide_write(device,ATA_REG_LBA4,(uint8_t)(lba >> 32));
		ide_write(device,ATA_REG_LBA5,(uint8_t)(lba >> 40));
		ide_write(device,ATA_REG_SECCOUNT0,(uint8_t)sectors_count);
		ide_write(device,ATA_REG_LBA0,(uint8_t)lba);
		ide_write(device,ATA_REG_LBA1,(uint8_t)(lba >> 8));
		ide_write(device,ATA_REG_LBA2,(uint8_t)(lba >> 16));
		ide_write(device,ATA_REG_COMMAND,ATA_CMD_READ_PIO_EXT);
	} else {
		//lba 28
		ide_write(device,ATA_REG_SECCOUNT0,(uint8_t)sectors_count);
		ide_write(device,ATA_REG_LBA0,(uint8_t)lba);
		ide_write(device,ATA_REG_LBA1,(uint8_t)(lba >> 8));
		ide_write(device,ATA_REG_LBA2,(uint8_t)(lba >> 16));
		ide_write(device,ATA_REG_COMMAND,ATA_CMD_READ_PIO);
	}

	uint16_t *buf = kmalloc(sectors_count * 512);

	for(size_t i=0; i<sectors_count; i++){
		//TODO : error handling ?
		ide_io_wait(device);
		if(ide_poll(device,ATA_SR_BSY,0) < 0){
			kfree(buf);
			release_mutex(&device->channel->lock);
			return -EIO;
		}
		for(size_t j=0; j<256; j++){
			buf[i * 256 + j] = in_word(device->channel->base + ATA_REG_DATA);
		}
	}
	ide_io_wait(device);
	release_mutex(&device->channel->lock);

	memcpy(buffer,((char *)buf) + offset % 512,count);
	kfree(buf);

	return count;
}

static int ide_ioctl(vfs_node *node,uint64_t req,void *arg){
	ide_device *device = node->private_inode;
	switch(req){
	case I_MODEL:
		strcpy(arg,device->model);
		return 0;
	default:
		return -EINVAL;
	}
}

static int ide_getattr(vfs_node *node,struct stat *st){
	ide_device *device = node->private_inode;
	//ata devices belong to root i guess ..
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_size = device->size;
	st->st_blksize = 512;
	st->st_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize;
	return 0;
}

static void ide_init_device(ide_device *device){
	//identify
	ide_write(device,ATA_REG_DRV_SELECT,device->drive);
	ide_io_wait(device);

	if(ide_poll(device,ATA_SR_BSY,0) < 0){
		device->exist = 0;
		return;
	}

	ide_write(device,ATA_REG_SECCOUNT0,0);
	ide_write(device,ATA_REG_LBA0,0);
	ide_write(device,ATA_REG_LBA1,0);
	ide_write(device,ATA_REG_LBA2,0);
	ide_write(device,ATA_REG_COMMAND,ATA_CMD_IDENTIFY);
	ide_io_wait(device);
	if(ide_read(device,ATA_REG_STATUS) == 0){
		//no drive
		device->exist = 0;
		return;
	}
	device->exist = 1;
	
	if(ide_poll(device,ATA_SR_BSY,0) < 0){
		device->exist = 0;
		return;
	}

	if(ide_read(device,ATA_REG_LBA1) || ide_read(device,ATA_REG_LBA2)){
		//atapi
		device->class = ATA_CLASS_ATAPI;
		kdebugf("find usable atapi drive on channel %s drive %s\n",device->channel->base == 0x1f0 ? "primary" : "secondary",device->drive == ATA_DRIVE_MASTER ? "master" : "slave");
		return;
	}
	device->class = ATA_CLASS_ATA;
	for(;;){
		uint8_t status = ide_read(device,ATA_REG_STATUS);
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

	//we want this shit to be aligned
	ata_ident ident;
	uint16_t *buf = (uint16_t *)&ident;
	for (size_t i = 0; i < 256; i++){
		buf[i] = in_word(device->channel->base + ATA_REG_DATA);
	}

	uint64_t sectors = ident.command_sets & (1 << 26) ? ident.sectors_lba48 : ident.sectors;
	for(size_t i=0; i<sizeof(ident.model); i+= 2){
		device->model[i + 1] = ident.model[i];
		device->model[i]     = ident.model[i + 1];
	}
	for(size_t i=sizeof(device->model)-1; i>0 && device->model[i] == ' '; i--){
		device->model[i] = '\0';
	}
	device->command_sets = ident.command_sets;

	kdebugf("model : %s commandsets : %x support lba48 : %s max lba : %ld\n",device->model,ident.command_sets,ident.command_sets & (1 << 26) ? "true" : "false",sectors);
	kdebugf("find usable ata drive on channel %s drive %s\n",device->channel->base == 0x1f0 ? "primary" : "secondary",device->drive == ATA_DRIVE_MASTER ? "master" : "slave");


	char path[256];
	sprintf(path,"/dev/hd%c",hdx++);
	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	device->node = node;
	device->size = sectors * 512;
	node->flags = VFS_BLOCK | VFS_DEV;
	node->private_inode = device;
	node->ioctl   = ide_ioctl;
	node->getattr = ide_getattr;
	node->read    = ata_read;
	vfs_mount(path,node);
}

static void check_dev(uint8_t bus,uint8_t device,uint8_t function,void *arg){
	(void)arg;
	uint8_t base_class = pci_read_config_byte(bus,device,function,PCI_CONFIG_BASE_CLASS);
	uint8_t sub_class  = pci_read_config_byte(bus,device,function,PCI_CONFIG_SUB_CLASS);
	uint8_t prog_if    = pci_read_config_byte(bus,device,function,PCI_CONFIG_PROG_IF);
	

	if(!((base_class == 1) && ((sub_class == 5) || (sub_class == 1)))){
		return;
	}
	kdebugf("find ata disk on %d:%d:%d\n",bus,device,function);
	if((prog_if & 0x1) || (prog_if & 0x2)){
		kdebugf("ata disk don't support compatibility mode\n");
		return;
	}

	uint32_t bar0 = pci_read_config_dword(bus,device,function,PCI_CONFIG_BAR0) & ~0x3;
	uint32_t bar1 = pci_read_config_dword(bus,device,function,PCI_CONFIG_BAR1) & ~0x3;
	uint32_t bar2 = pci_read_config_dword(bus,device,function,PCI_CONFIG_BAR2) & ~0x3;
	uint32_t bar3 = pci_read_config_dword(bus,device,function,PCI_CONFIG_BAR3) & ~0x3;
	uint32_t bar4 = pci_read_config_dword(bus,device,function,PCI_CONFIG_BAR4) & ~0x3;

	ide_controller *controller = kmalloc(sizeof(ide_controller));
	memset(controller,0,sizeof(ide_controller));

	controller->channel[0].base  = bar0 ? bar0 : 0x1f0;
	controller->channel[1].base  = bar2 ? bar2 : 0x170;
	controller->channel[0].ctrl  = bar1 ? bar1 : 0x3f6;
	controller->channel[1].ctrl  = bar3 ? bar3 : 0x376;
	controller->channel[0].bmide = bar4;
	controller->channel[1].bmide = bar4 + 8;
	controller->channel[0].nIEN  = 0x2;
	controller->channel[1].nIEN  = 0x2;
	init_mutex(&controller->channel[0].lock);
	init_mutex(&controller->channel[1].lock);

	controller->devices[0].channel = &controller->channel[0];
	controller->devices[1].channel = &controller->channel[0];
	controller->devices[2].channel = &controller->channel[1];
	controller->devices[3].channel = &controller->channel[1];
	controller->devices[0].drive = ATA_DRIVE_MASTER;
	controller->devices[1].drive = ATA_DRIVE_SLAVE;
	controller->devices[2].drive = ATA_DRIVE_MASTER;
	controller->devices[3].drive = ATA_DRIVE_SLAVE;
	
	//soft reset the devices first
	for (size_t i = 0; i < 4; i++){
		ide_reset(&controller->devices[i]);
	}
	for (size_t i = 0; i < 4; i++){
		ide_init_device(&controller->devices[i]);
	}

	//if no device on controller just ignore it
	for(size_t i=0; i<5; i++){
		if(i == 5){
			kfree(controller);
			return;
		}
		if(controller->devices[i].exist)break;
	}
	
	list_append(ide_controllers,controller);
}

int ide_init(int argc,char **argv){
	(void)argc;
	(void)argv;
	ide_controllers = new_list();
	pci_foreach(check_dev,NULL);
	if(!ide_controllers->node_count){
		kdebugf("no ata disk found\n");
		//return an error
		// so the module loader remove us
		return -ENODEV;
	}
	return 0;
}

int ide_fini(){
	free_list(ide_controllers);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = ide_init,
	.fini = ide_fini,
	.author = "tayoky",
	.name = "ata",
	.description = "ata pio driver",
	.license = "GPL 3",
};