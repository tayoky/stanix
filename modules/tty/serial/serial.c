#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/vfs.h>
#include <kernel/device.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <kernel/irq.h>
#include <kernel/arch.h>
#include <kernel/port.h>
#include <kernel/tty.h>
#include <errno.h>

//this is the real serial port driver
//kernel/arch/x86_64/serial.c is only for early debugging

#define SERIAL_DATA 0
#define SERIAL_IER 1 //Interrupt enable register
#define SERIAL_FCR 2
#define SERIAL_LCR 3

#define SERIAL_MCR      4        //Modem Status Register. 
#define SERIAL_MCR_DTR  1UL << 0 //Controls the Data Terminal Ready Pin 
#define SERIAL_MCR_RTS  1UL << 1 //Controls the Request to Send Pin 
#define SERIAL_MCR_OUT1 1UL << 2 //Controls a hardware pin (OUT1) which is unused in PC implementations 
#define SERIAL_MCR_OUT2 1UL << 3 //Controls a hardware pin (OUT2) which is used to enable the IRQ in PC implementations 
#define SERIAL_MCR_LOOP 1UL << 4 //Provides a local loopback feature for diagnostic testing of the UART 

#define SERIAL_LSR 5 //Line Status Register
#define SERIAL_LSR_DR   1UL << 0 //Set if there is data that can be read
#define SERIAL_LSR_THRE 1UL << 5 //Set if the transmission buffer is empty (i.e. data can be sent)

#define SERIAL_SCR 7

typedef struct serial {
	tty_t tty;
	resource_t *io_res;
	resource_t *irq_res;
	void *handler_handle;
} serial_t;

static void serial_handler(registers_t *frame, void *data) {
	(void)frame;
	serial_t *serial = data;
	
	tty_input(&serial->tty, resource_read8(serial->io_res, SERIAL_DATA));
}

static ssize_t serial_out(tty_t *tty, const char *buf, size_t size) {
	serial_t *serial = container_of(tty, serial_t, tty);
	for (size_t i=0; i<size; i++) {
		while (!(resource_read8(serial->io_res, SERIAL_LSR) & SERIAL_LSR_THRE));
		resource_write8(serial->io_res, SERIAL_DATA, buf[i]);
	}
	return size;
}

static tty_ops_t serial_ops = {
	.out = serial_out,
};

static int serial_check(devnode_t *devnode) {
	// we need resource for our test
	resource_t *io_res = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, RID_ANY);
	if (!io_res) return 0;

	int ret = 1;

	// save old value
	uint8_t orig = resource_read8(io_res, SERIAL_SCR);

	// test 1
	resource_write8(io_res, SERIAL_SCR, 0xa5);
	if (resource_read8(io_res, SERIAL_SCR) != 0xa5) {
		ret = 0;
		goto finish;
	}

	// test 2
	resource_write8(io_res, SERIAL_SCR, 0x5a);
	if (resource_read8(io_res, SERIAL_SCR) != 0x5a) {
		ret = 0;
		goto finish;
	}

finish:

	// restore saved value
	resource_write8(io_res, SERIAL_SCR, orig);

	device_release_resource(devnode, io_res);
	return ret;
}

static int serial_probe(devnode_t *devnode) {
	int ret = 0;
	serial_t *serial = kmalloc(sizeof(serial_t));
	if (!serial) return -ENOMEM;
	memset(serial, 0, sizeof(serial_t));

	// get resources
	serial->io_res  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, RID_ANY);
	if (IS_ERR(serial->io_res)) {
		ret = PTR2ERR(serial->io_res);
		goto error;
	}
	serial->irq_res = device_allocate_simple_resource(devnode, RESOURCE_IRQ, RID_ANY);
	if (IS_ERR(serial->irq_res)) {
		ret = PTR2ERR(serial->irq_res);
		goto error;
	}

	resource_write8(serial->io_res, SERIAL_IER, 0x01);
	resource_write8(serial->io_res, SERIAL_LCR, 0x80);
	resource_write8(serial->io_res, SERIAL_DATA, 0x03);
	resource_write8(serial->io_res, SERIAL_IER, 0x00);
	resource_write8(serial->io_res, SERIAL_LCR, 0x03);
	resource_write8(serial->io_res, SERIAL_FCR, 0b111);
	resource_write8(serial->io_res, SERIAL_MCR, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
	resource_write8(serial->io_res, SERIAL_MCR, SERIAL_MCR_LOOP | SERIAL_MCR_OUT1 | SERIAL_MCR_OUT2 | SERIAL_MCR_RTS);
	resource_write8(serial->io_res, SERIAL_DATA, 0xae);
	if (resource_read8(serial->io_res, SERIAL_DATA) != 0xae) {
		ret = -ENODEV;
		goto error;
	}
	resource_write8(serial->io_res, SERIAL_MCR, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT1 | SERIAL_MCR_OUT2);

	new_tty(&serial->tty);
	serial->tty.ops = &serial_ops;

	serial->handler_handle = resource_register_handler(serial->irq_res, serial_handler, serial);
	if (!serial->handler_handle) {
		ret = -EIO;
		goto error;
	}

	ret = device_register(&serial->tty.device, "ttyS%d", 0);
	if (ret < 0) goto error;

	return 0;

error:
	resource_unregister_handler(serial->io_res, serial->handler_handle);
	device_release_resource(devnode, serial->irq_res);
	device_release_resource(devnode, serial->io_res);
	kfree(serial);
	return ret;
}

static driver_t serial_driver = {
	.name = "serial port",
	.device_name = "serial%d",
	.buses = BUSES("root", "pci", "isa"),
	.check = serial_check,
	.probe = serial_probe,
};

static int serial_init(int argc,char **argv) {
	driver_register(&serial_driver);

	// TODO : use isa bus instead
	// this is used as a proof of concept
	devnode_t *device = device_allocate();
	bus_add_resource_desc(device, RESOURCE_IOPORT, RID_ANY, 0x3f8, 8);
	irq_t *irq = irq_get_from_hwirq(main_irq_chip, 4);
	bus_add_resource_desc_data(device, RESOURCE_IRQ, RID_ANY, irq, 1);
	bus_attach_child(bus_get_root(), device, "serial%d", 0);
}

static int serial_fini() {
	driver_unregister(&serial_driver);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = serial_init,
	.fini = serial_fini,
	.name = "serial",
	.description = "COM serial port driver",
	.author = "tayoky",
	.license = "GPL 3"
};
