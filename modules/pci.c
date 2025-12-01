#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/devfs.h>
#include <kernel/port.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <module/pci.h>
#include <stdint.h>
#include <errno.h>

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC

/// @brief take an device bus function and offset and turn it into an conf addr
/// @param bus 
/// @param device 
/// @param function 
/// @param offset 
/// @return the configuration address
static inline uint32_t pci_dev2conf_addr(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset){
	return (((uint32_t)bus) << 16) | (((uint32_t)device) << 11) | (((uint32_t)function) << 8) | offset | ((uint32_t)0x80000000);
}

uint32_t pci_read_config_dword(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset){
	uint32_t addr = pci_dev2conf_addr(bus,device,function,offset);

	//write the address with the two last bit alaways 0
	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	return in_long(CONFIG_DATA);
}

void pci_write_config_dword(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint32_t data){
	uint32_t addr = pci_dev2conf_addr(bus,device,function,offset);

	//write the address with the two last bit alaways 0
	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	out_long(CONFIG_DATA,data);
}

uint16_t pci_read_config_word(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset){
	uint32_t addr = pci_dev2conf_addr(bus,device,function,offset);

	//write the address with the two last bit alaways 0
	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	//shift to get the good word
	return (uint16_t)((in_long(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}


void pci_write_config_word(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint16_t data){
	uint32_t addr = pci_dev2conf_addr(bus,device,function,offset);

	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	uint32_t dword = in_long(CONFIG_DATA);
	dword &= (0xffff << ((offset & 2) * 8));
	dword |= data << ((offset & 2) * 8);
	
	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	out_long(CONFIG_DATA,data);
}

uint8_t pci_read_config_byte(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset){
	uint32_t addr = pci_dev2conf_addr(bus,device,function,offset);

	//write the address with the two last bit alaways 0
	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	//shift to get the good word
	return (uint8_t)((in_long(CONFIG_DATA) >> ((offset & 0b11) * 8)) & 0xFF);
}

void pci_write_config_byte(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint8_t data){
	uint32_t addr = pci_dev2conf_addr(bus,device,function,offset);

	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	uint32_t dword = in_long(CONFIG_DATA);
	dword &= (0xff << ((offset & 2) * 8));
	dword |= data << ((offset & 2) * 8);
	
	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	out_long(CONFIG_DATA,data);
}

static void check_bus(uint8_t bus,void (*func)(uint8_t,uint8_t,uint8_t,void *),void *);

static void check_function(uint8_t bus, uint8_t device, uint8_t function,void (*func)(uint8_t,uint8_t,uint8_t,void *),void *arg){
	if(func){
		func(bus,device,function,arg);
	}

	uint8_t base_class = (pci_read_config_word(bus,device,function,PCI_CONFIG_CLASS) >> 8) & 0xFF;
	uint8_t sub_class = pci_read_config_word(bus,device,function,PCI_CONFIG_CLASS) & 0xFF;
	uint8_t secondary_bus;

	//check for PCI to PCI bridge
	if ((base_class == 0x6) && (sub_class == 0x4)) {
		secondary_bus = (pci_read_config_word(bus,device,function,PCI_CONFIG_BUS_NUMBER) >> 8) & 0xFF;
		check_bus(secondary_bus,func,arg);
	}
}

static void check_device(uint8_t bus,uint8_t device,void (*func)(uint8_t,uint8_t,uint8_t,void *),void  *arg){
	uint8_t function = 0;
	
	//read the vendor id of the device
	uint16_t vendorID= pci_read_config_word(bus,device,function,PCI_CONFIG_VENDOR_ID);
	if (vendorID == 0xFFFF){
		//device doesn't exist
		return;
	}

	//now we know the device exist check every single function of the device
	check_function(bus,device,function,func,arg);
	//read the header type
	uint32_t header_type = pci_read_config_word(bus,device,function,PCI_CONFIG_HEADER_TYPE);
	if(header_type & 0x80){
		//it's a multi-function device, so check remaining functions
		for (function = 1; function < 8; function++) {
			if (pci_read_config_word(bus,device,function,PCI_CONFIG_VENDOR_ID) != 0xFFFF) {
				check_function(bus,device,function,func,arg);
			}
		}
	}
}

static void check_bus(uint8_t bus,void (*func)(uint8_t,uint8_t,uint8_t,void *),void *arg){
	uint8_t device;

	for (device = 0; device < 32; device++) {
		check_device(bus,device,func,arg);
	}
}

void pci_foreach(void (*func)(uint8_t,uint8_t,uint8_t,void *),void *arg){
	uint8_t function;

	uint32_t headerType = pci_read_config_word(0,0,0,PCI_CONFIG_HEADER_TYPE);
	if ((headerType & 0x80) == 0) {
		//single PCI host controller
		check_bus(0,func,arg);
	} else {
		//multiple PCI host controllers
		//each function belong to a PCI host controller
		for (function = 0; function < 8; function++) {
			if(pci_read_config_word(0,0,function,PCI_CONFIG_VENDOR_ID) != 0xFFFF){
				check_bus(function,func,arg);
			}
		}
	}
}

typedef struct {
	uint8_t bus;
	uint8_t device;
	uint8_t function;
} pci_inode;

static ssize_t pci_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	pci_inode *inode = node->private_inode;

	//config space is 256 bytes
	if(offset >= 256){
		return 0;
	}
	if(offset + count >= 256){
		count = 256 - offset;
	}

	for (size_t i = 0; i < count; i++){
		*(uint8_t *)buffer = pci_read_config_byte(inode->bus,inode->device,inode->function,offset);
		offset ++;
		(uint8_t *)buffer++;
	}

	return count;
}

static void create_pci_dev(uint8_t bus,uint8_t device,uint8_t function,void *arg){
	(void)arg;
	uint16_t vendorID = pci_read_config_word(bus,device,function,PCI_CONFIG_VENDOR_ID);
	uint16_t deviceID = pci_read_config_word(bus,device,function,PCI_CONFIG_DEVICE_ID);
	kdebugf("pci : find bus %d device %d function %d vendorID : %lx deviceID : %lx\n",bus,device,function,vendorID,deviceID);

	char path[32];
	sprintf(path,"pci/%02d:%d:%d",bus,device,function);

	//setup the vnode
	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->flags = VFS_DEV | VFS_BLOCK;
	node->read = pci_read;

	//setup inode
	pci_inode *inode = kmalloc(sizeof(pci_inode));
	memset(inode,0,sizeof(pci_inode));
	inode->bus      = bus;
	inode->device   = device;
	inode->function = function;
	node->private_inode = inode;

	if(devfs_create_dev(path,node)){
		kdebugf("pci : fail to mount %s\n",path);
	}
}

int init_pci(int argc,char **argv){
	(void)argc;
	(void)argv;

	vfs_mkdir("/dev/pci",0755);

	pci_foreach(create_pci_dev,NULL);
	
	EXPORT(pci_foreach);
	EXPORT(pci_read_config_dword)
	EXPORT(pci_read_config_word)
	EXPORT(pci_read_config_byte)
	EXPORT(pci_write_config_dword)
	EXPORT(pci_write_config_word)
	EXPORT(pci_write_config_byte)
	return 0;
}

int rm_pci(){
	UNEXPORT(pci_foreach);
	UNEXPORT(pci_read_config_dword)
	UNEXPORT(pci_read_config_word)
	UNEXPORT(pci_read_config_byte)
	UNEXPORT(pci_write_config_dword)
	UNEXPORT(pci_write_config_word)
	UNEXPORT(pci_write_config_byte)
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_pci,
	.fini = rm_pci,
	.name = "pci",
	.author = "tayoky",
	.description = "pci driver",
	.license = "GPL 3",
};