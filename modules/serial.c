#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/irq.h>
#include <kernel/arch.h>
#include <kernel/port.h>
#include <errno.h>

//this is the real serial port driver
//kernel/arch/x86_64/serial.c is only for early debugging

#define SERIAL_DATA 0

#define SERIAL_IER 1 //Interrupt enable register

#define SERIAL_LSR 5 //Line Status Register
#define SERIAL_LSR_DR   1UL << 0 //Set if there is data that can be read
#define SERIAL_LSR_THRE 1UL << 5 //Set if the transmission buffer is empty (i.e. data can be sent)

#define SERIAL_MCR      4        //Modem Status Register. 
#define SERIAL_MCR_DTR  1UL << 0 //Controls the Data Terminal Ready Pin 
#define SERIAL_MCR_RTS  1UL << 1 //Controls the Request to Send Pin 
#define SERIAL_MCR_OUT1 1UL << 2 //Controls a hardware pin (OUT1) which is unused in PC implementations 
#define SERIAL_MCR_OUT2 1UL << 3 //Controls a hardware pin (OUT2) which is used to enable the IRQ in PC implementations 
#define SERIAL_MCR_LOOP 1UL << 4 //Provides a local loopback feature for diagnostic testing of the UART 

static uint16_t str2port(char *str){
	if((strlen(str) != 4) || (memcmp(str,"COM",3) && memcmp(str,"com",3))){
		return 0;
	}
	switch (str[3]){
	case '1':
		return 0x3F8;
	case '2':
		return 0x2F8;
	case '3':
		return 0x3E8;
	case '4':
		return 0x2E8;
	case '5':
		return 0x5F8;
	case '6':
		return 0x5E8;
	case '7':
		return 0x5E8;
	case '8':
		return 0x4E8;
	default:
		return 0;
	}
}

int serial_count = 1;

static ssize_t serial_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	uint16_t port = (uint16_t)node->private_inode;

	char *str = buffer;
	for (size_t i = 0; i < count; i++){
		while (!(in_byte(port + SERIAL_LSR) & SERIAL_LSR_DR));
		*str = in_byte(port);
		if(*str == '\r'){
			*str = '\n';
		}
		while (!(in_byte(port + SERIAL_LSR) & SERIAL_LSR_THRE));
		out_byte(port,*str);
		str++;
	}

	return count;
}

static ssize_t serial_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	uint16_t port = (uint16_t)node->private_inode;

	char *str = buffer;
	for (size_t i = 0; i < count; i++){
		while (!(in_byte(port + SERIAL_LSR) & SERIAL_LSR_THRE));
		out_byte(port,*str);
		str++;	
	}

	return count;
}

static void serial_handler(fault_frame *frame){
	(void)frame;
	kdebugf("serial handler triggerd\n");
}

static int init_port(uint16_t port){
	out_byte(port + SERIAL_IER, 0x00);
	out_byte(port + 3, 0x80);
	out_byte(port + SERIAL_DATA, 0x03);
	out_byte(port + SERIAL_IER, 0x00);
	out_byte(port + 3, 0x03);
	out_byte(port + 2, 0xC7);
	out_byte(port + SERIAL_MCR, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
	out_byte(port + SERIAL_MCR, SERIAL_MCR_LOOP | SERIAL_MCR_OUT1 | SERIAL_MCR_OUT2 | SERIAL_MCR_RTS);
	out_byte(port + SERIAL_DATA, 0xAE);
	if(in_byte(port + SERIAL_DATA) != 0xAE) {
		return -ENODEV;
	}
	out_byte(port + SERIAL_MCR, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT1 | SERIAL_MCR_OUT2);

	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->read = serial_read;
	node->write = serial_write;
	node->private_inode = (void *)port;

	char path[20];
	sprintf(path,"/dev/ttyS%d",serial_count);
	kdebugf("serial : mounting serial port under %s\n",path);
	if(vfs_mount(path,node)){
		return -EIO;
	}

	irq_generic_map(serial_handler,4);

	serial_count++;
	return 0;
}

static int init_serial(int argc,char **argv){
	serial_count = 0;
	if(have_opt(argc - 1,argv,"--port")){
		int count = 0;
		for (int i = 0; i < argc-1; i++){
			if(!strcmp("--port",argv[i])){
				init_port(str2port(argv[i+1]));
			}
			
		}
		if(!serial_count){
			return -ENODEV;
		}
		return 0;
	} else {
		//default to COM1
		return init_port(str2port("COM1"));
	}
}

static int fini_serial(){

}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_serial,
	.fini = fini_serial,
	.name = "serial",
	.description = "COM serial port driver",
	.author = "tayoky",
	.license = "GPL 3"
};