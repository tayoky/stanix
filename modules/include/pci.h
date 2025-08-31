#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_VENDOR_ID   0x00
#define PCI_CONFIG_DEVICE_ID   0x02
#define PCI_CONFIG_COMMAND     0x04
#define PCI_CONFIG_STATUS      0x06
#define PCI_CONFIG_PROG_IF     0x09
#define PCI_CONFIG_CLASS       0x0A
#define PCI_CONFIG_SUB_CLASS   0x0A
#define PCI_CONFIG_BASE_CLASS  0x0B
#define PCI_CONFIG_HEADER_TYPE 0x0E

//header type 0x0 
#define PCI_CONFIG_BAR0        0x10
#define PCI_CONFIG_BAR1        0x14
#define PCI_CONFIG_BAR2        0x18
#define PCI_CONFIG_BAR3        0x20
#define PCI_CONFIG_BAR4        0x22
#define PCI_CONFIG_BAR5        0x24
#define PCI_CONFIG_INT_LINE    0x3C
#define PCI_CONFIG_INT_PIN     0x3D

//header type 0x01 (PCI to PCi bridge)
#define PCI_CONFIG_BUS_NUMBER  0x18

/// @brief read a aligned dword from a pci device configuration space
/// @param bus the bus device is on
/// @param device the slot of the device onto the bus
/// @param function the selected function of the device
/// @param offset address to read (inside the configuration space) MUST BE ALIGNED
/// @return the dword read from the configuration space
uint32_t pci_read_config_dword(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset);

/// @brief read a aligned word from a pci device configuration space
/// @param bus the bus device is on
/// @param device the slot of the device onto the bus
/// @param function the selected function of the device
/// @param offset address to read (inside the configuration space) MUST BE ALIGNED
/// @return the word read from the configuration space
uint16_t pci_read_config_word(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset);

/// @brief read a byte from a pci device configuration space
/// @param bus the bus device is on
/// @param device the slot of the device onto the bus
/// @param function the selected function of the device
/// @param offset address to read (inside the configuration space)
/// @return the word read from the configuration space
uint8_t pci_read_config_byte(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset);


void pci_write_config_dword(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint32_t data);
void pci_write_config_word(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint16_t data);
void pci_write_config_byte(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint8_t data);

/// @brief call a function for each pci device
/// @param func the function called for each pci device
/// @param arg an argument pass to the function
void pci_foreach(void (*func)(uint8_t,uint8_t,uint8_t,void *),void *arg);

#endif