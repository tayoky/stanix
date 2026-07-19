#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/ringbuf.h>
#include <kernel/port.h>
#include <kernel/bus.h>
#include <kernel/irq.h>
#include <module/ps2.h>
#include <errno.h>

//driver for the 8042 ps2 controller
//work only on x86_64 computer

#define I8042_DATA    0x60
#define I8042_COMMAND 0x64
#define I8042_STATUS  0x64

// i8042 controller command

#define I8042_DISABLE_PORT1   0xAD
#define I8042_ENABLE_PORT1    0xAE
#define I8042_DISABLE_PORT2   0xA7
#define I8042_ENABLE_PORT2    0xA8
#define I8042_TEST_CONTROLLER 0xAA
#define I8042_TEST_PORT1      0xAB
#define I8042_TEST_PORT2      0xA9
#define I8042_READ_CCB        0x20
#define I8042_WRITE_CCB       0x60
#define I8042_SEND_PORT2      0xD4

#define I8042_CONTROLLER_TEST_SUCCEED 0x55
#define I8042_CONTROLLER_TEST_FAILED  0xFC

int have_ports[2] = { 1, 0 };
static ps2_addr_t ports[2];
static bus_ops_t ps2_ops;
static device_driver_t ps2_driver = {
	.name = "8042",
};
static bus_t ps2_bus = {
	.device = {
		.name = "ps2",
		.driver = &ps2_driver,
		.type = DEVICE_BUS,
	},
	.ops = &ps2_ops,
};

// i8042 specific I/O

static int i8042_wait_output(void) {
	for (size_t i = 0; i < 100000; i++) {
		if (in_byte(I8042_STATUS) & 0x01) {
			return 0;
		}
		io_wait();
	}

	return -ETIMEDOUT;
}

static int i8042_wait_input(void) {
	for (size_t i = 0; i < 100000; i++) {
		if (!(in_byte(I8042_STATUS) & 0x02)) {
			return 0;
		}
		io_wait();
	}

	return -ETIMEDOUT;
}

static int i8042_send_command(uint8_t command) {
	int ret = i8042_wait_input();
	if (ret < 0) return ret;
	out_byte(I8042_COMMAND, command);
	return 0;
}

static int i8042_read(void) {
	int ret = i8042_wait_output();
	if (ret < 0) return ret;
	return in_byte(I8042_DATA);
}

static int i8042_write(uint8_t data) {
	int ret = i8042_wait_input();
	if (ret < 0) return ret;
	out_byte(I8042_DATA, data);
	return 0;
}

int i8042_read_ccb(uint8_t *ccb) {
	int ret = i8042_send_command(I8042_READ_CCB);
	if (ret < 0) return ret;
	ret = i8042_read();
	if (ret < 0) return ret;
	*ccb = (uint8_t)ret;
	return 0;
}

int i8042_write_ccb(uint8_t ccb) {
	int ret = i8042_send_command(I8042_WRITE_CCB);
	if (ret < 0) return ret;
	return i8042_write(ccb);
}

void i8042_flush(void) {
	while (in_byte(I8042_STATUS) & 1) in_byte(I8042_DATA);
}

// generic ps2 I/O

int ps2_read(void) {
	return i8042_read();
}

static int ps2_write(uint8_t data) {
	return i8042_write(data);
}

int ps2_send(uint8_t port, uint8_t data) {
	for (int i=0; i < 3; i++) {
		if (port == 2) {
			int ret = i8042_send_command(I8042_SEND_PORT2);
			if (ret < 0) return ret;
		}
		int ret = ps2_write(data);
		if (ret < 0) return ret;

		ret = ps2_read();
		if (ret < 0) return ret;
		if (ret != PS2_RESEND) return ret;
		// retry
	}
	return -ETIMEDOUT;
}

int ps2_reset(uint8_t port) {
	int ret = ps2_send(port, PS2_RESET);
	if (ret < 0)  return ret;
	if (ret != PS2_ACK) return -EIO;

	ret = ps2_read();
	if (ret < 0) return ret;
	if (ret != PS2_SELF_TEST_PASSED) return -EIO;

	// discard the id
	int c0 = ps2_read();
	if (c0 == 0xAB || c0 == 0xAC) {
		ps2_read();
	}
	return 0;
}

static ssize_t ps2_bus_read(bus_addr_t *addr, void *buf, off_t offset, size_t count) {
	(void)addr;
	(void)offset;
	unsigned char *c = buf;
	ssize_t total = 0;
	while (count > 0) {
		int byte = ps2_read();
		if (byte < 0) break;
		*c = (unsigned char)byte;
		count--;
		total++;
		c++;
	}
	return total;
}

static int ps2_register_handler(bus_addr_t *addr, interrupt_handler_t handler, void *data) {
	ps2_addr_t *ps2_addr = (ps2_addr_t *)addr;
	switch (ps2_addr->port) {
	case 1:
		irq_register_handler(irq_hirq2irq(1), handler, data);
		return 0;
	case 2:
		irq_register_handler(irq_hirq2irq(12), handler, data);
		return 0;
	default:
		return -EINVAL;
	}
}

static bus_ops_t ps2_ops = {
	.read = ps2_bus_read,
	.register_handler = ps2_register_handler,
};

static void print_device_name(int port) {
	if (port == 1) {
		kdebugf("ps2 : first port device : ");
	} else {
		kdebugf("ps2 : second port device : ");
	}

	if (ps2_send(port, PS2_IDENTIFY) != PS2_ACK) {
		kprintf("unknown device\n");
	}

	int c0 = ps2_read();
	int c1 = -1;
	if (c0 == 0xAB || c0 == 0xAC) {
		c1 = ps2_read();
	}

	ports[port - 1].device_id[0] = c0;
	ports[port - 1].device_id[1] = c1;

	switch (c0) {
	case -1: //-1 mean no byte
		kprintf("Ancient AT keyboard\n");
		break;
	case 0x00:
		kprintf("Standard PS/2 mouse\n");
		break;
	case 0x03:
		kprintf("Mouse with scroll wheel\n");
		break;
	case 0x04:
		kprintf("5-button mouse\n");
		break;
	case 0xAB:
		switch (c1) {
		case 0x83:
		case 0xC1:
			kprintf("MF2 keyboard\n");
			break;
		case 0x84:
			kprintf("Short Keyboard\n");
			break;
		case 0x85:
			kprintf("122-Key Host Connect(ed) Keyboard\n");
			break;
		case 0x86:
			kprintf("122-key keyboards\n");
			break;
		default:
			kprintf("unknown keyboard %x:%x\n", c0, c1);
			break;
		}
		break;
	case 0xAC:
		switch (c1) {
		case 0xA1:
			kprintf("NCD Sun layout keyboard\n");
			break;
		default:
			kprintf("unknown device\n");
			break;
		}
		break;
	default:
		kprintf("unknown device : %x:%x\n", c0, c1);
		break;
	}

}

static void setup_addr(int port) {
	list_append(&ps2_bus.addresses, &ports[port - 1].addr.node);
	char name[32];
	sprintf(name, "port%d", port);
	ports[port - 1].addr.type = BUS_PS2;
	ports[port - 1].addr.name = strdup(name);
	ports[port - 1].addr.bus  = &ps2_bus;
	ports[port - 1].port = port;
}

static int init_i8042(int argc, char **argv) {
	// disable everything
	i8042_send_command(I8042_DISABLE_PORT1);
	i8042_send_command(I8042_DISABLE_PORT2);

	i8042_flush();

	// test the controller
	i8042_send_command(I8042_TEST_CONTROLLER);
	if (i8042_read() != I8042_CONTROLLER_TEST_SUCCEED) {
		kdebugf("ps2 : the 8042 ps2 controller didn't pass self test (broken controller ?)\n");
		return -ENODEV;
	}

	// try to check for port 2
	i8042_send_command(I8042_ENABLE_PORT2);
	uint8_t conf;
	i8042_read_ccb(&conf);
	if (!(conf & (1 << 5))) {
		// there is a second port
		have_ports[1] = 1;
	}
	i8042_send_command(I8042_DISABLE_PORT2);

	// test ports
	i8042_send_command(I8042_TEST_PORT1);
	if (i8042_read() != 0) {
		have_ports[0] = 0;
		kdebugf("ps2 : the first ps2 port didn't pass test (broken controller ?)\n");
	}
	if (have_ports[1]) {
		i8042_send_command(I8042_TEST_PORT2);
		if (i8042_read() != 0) {
			have_ports[1] = 0;
			kdebugf("ps2 : the second ps2 port didn't pass test (broken controller or non present aux port ?)\n");
		}
	}

	// if no port available just give up
	if (!(have_ports[0] || have_ports[1])) {
		kdebugf("ps2 : both ps2 ports are not available\n");
		return -ENODEV;
	}

	// setup the configuration byte
	i8042_read_ccb(&conf);

	// start by setting fields to 0
	conf &= ~1;
	conf &= ~2;
	conf &= ~0x40;

	// then activate irq
	if (have_ports[0]) {
		conf |= 1;
	}
	if (have_ports[1]) {
		conf |= 2;
	}

	// activate devices
	if (have_ports[0]) {
		i8042_send_command(I8042_ENABLE_PORT1);
	}
	if (have_ports[1]) {
		i8042_send_command(I8042_ENABLE_PORT2);
	}

	// setup driver and bus
	device_driver_register(&ps2_driver);

	// now scan the device on each port
	for (int i=1; i < 3; i++) {
		if (!have_ports[i - 1]) continue;
		if (ps2_send(i, PS2_DISABLE_SCANNING) != PS2_ACK) {
			// no device on the port
			have_ports[i - 1] = 0;
			kdebugf("ps2 : no device on port %d\n", i);
		} else {
			// identify the device
			print_device_name(i);
			setup_addr(i);
		}
	}

	// we now want to enable translation
	if (!have_opt(argc, argv, "--no-translation")) {
		conf |= 0x40;
	}

	// now write conf
	i8042_write_ccb(conf);

	device_register((device_t *)&ps2_bus);

	kdebugf("ps2 : 8042 ps2 controller initialized\n");

	// NOTE : at this point scanning is disable
	// the driver specfic to the device as to enable scanning itself

	// export time
	EXPORT(ps2_read);
	EXPORT(ps2_send);
	EXPORT(ps2_reset);

	return 0;
}

static int fini_i8042() {
	device_destroy((device_t *)&ps2_bus);
	device_driver_unregister(&ps2_driver);
	UNEXPORT(ps2_read);
	UNEXPORT(ps2_send);
	UNEXPORT(ps2_reset);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_i8042,
	.fini = fini_i8042,
	.name = "i8042 controller driver",
	.author = "tayoky"
};
