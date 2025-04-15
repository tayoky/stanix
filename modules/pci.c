#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/vfs.h>
#include <kernel/port.h>
#include <stdint.h>

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC

#define PCI_CONFIG_VENDOR_ID   0x00
#define PCI_CONFIG_DEVICE_ID   0x02
#define PCI_CONFIG_COMMAND     0x04
#define PCI_CONFIG_STATUS      0x06
#define PCI_CONFIG_CLASS       0x0A
#define PCI_CONFIG_HEADER_TYPE 0x0E
#define PCI_CONFIG_BUS_NUMBER  0x18

/// @brief take an device bus function and offset and turn it into an conf addr
/// @param bus 
/// @param device 
/// @param function 
/// @param offset 
/// @return the configuration address
static inline uint32_t pci_dev2conf_addr(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset){
	return (((uint32_t)bus) << 16) | (((uint32_t)device) << 11) | (((uint32_t)function) << 8) | offset | ((uint32_t)0x80000000);
}

uint16_t pci_read_config_word(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset){
	uint32_t addr = pci_dev2conf_addr(bus,device,function,offset);

	//write the address with the two last bit alaways 0
	out_long(CONFIG_ADDRESS,addr  & (~(uint32_t)0b11));

	//shift to get the good word
	return (uint16_t)((in_long(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

static void check_bus(uint8_t bus);

static void check_function(uint8_t bus, uint8_t device, uint8_t function){
	kdebugf("find pci function %d:%d:%d\n",bus,device,function);

	uint8_t base_class = (pci_read_config_word(bus,device,function,PCI_CONFIG_CLASS) >> 8) & 0xFF;
	uint8_t sub_class = pci_read_config_word(bus,device,function,PCI_CONFIG_CLASS) & 0xFF;
	uint8_t secondary_bus;

	//check for PCI to PCI bridge
	if ((base_class == 0x6) && (sub_class == 0x4)) {
		secondary_bus = (pci_read_config_word(bus,device,function,PCI_CONFIG_BUS_NUMBER) >> 8) & 0xFF;
		check_bus(secondary_bus);
	}
}

static void check_device(uint8_t bus, uint8_t device){
	uint8_t function = 0;
	
	//read the vendor id of the device
	uint16_t vendorID= pci_read_config_word(bus,device,function,PCI_CONFIG_VENDOR_ID);
	if (vendorID == 0xFFFF){
		//device doesn't exist
		return;
	}

	//now we know the device exist check every single function of the device
	check_function(bus, device, function);
	//read the header type
	uint32_t header_type = pci_read_config_word(bus,device,function,PCI_CONFIG_DEVICE_ID);
	if( (header_type & 0x80)) {
		//it's a multi-function device, so check remaining functions
		for (function = 1; function < 8; function++) {
			if (pci_read_config_word(bus,device,function,PCI_CONFIG_VENDOR_ID) != 0xFFFF) {
				check_function(bus, device, function);
			}
		}
	}
}

static void check_bus(uint8_t bus){
	uint8_t device;

	for (device = 0; device < 32; device++) {
		check_device(bus, device);
	}
}

static void check_all_buses(){
	uint8_t function;

	uint32_t headerType = pci_read_config_word(0,0,0,PCI_CONFIG_HEADER_TYPE);
	if ((headerType & 0x80) == 0) {
		//single PCI host controller
		check_bus(0);
	} else {
		//multiple PCI host controllers
		//each function belong to a PCI host controller
		for (function = 0; function < 8; function++) {
			if (pci_read_config_word(0,0,function,PCI_CONFIG_VENDOR_ID) != 0xFFFF) break;
			check_bus(function);
		}
	}
}

int init_pci(int argc,char **argv){
	(void)argc;
	(void)argv;
	check_all_buses();
	return 0;
}

int rm_pci(){
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_pci,
	.fini = rm_pci,
	.name = "pci",
	.author = "tayoky",
};