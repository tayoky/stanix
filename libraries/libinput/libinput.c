#include <libinput.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

int libinput_open(const char *path, int flags) {
	int fd = open(path, O_RDONLY | flags);
	if (fd < 0) return fd;
	int ret;
	if ((ret = ioctl(fd, I_INPUT_GET_CONTROL, NULL)) < 0) {
		close(fd);
		return ret;
	}
	return fd;
}

int libinput_close(int fd) {
	// the kernel will drod control anyway but just be clean
	ioctl(fd, I_INPUT_DROP_CONTROL, NULL);
	return close(fd);
}

int libinput_get_event(int fd, struct input_event *event) {
	ssize_t ret = read(fd, event, sizeof(struct input_event));
	if (ret > 0) {
		ret = 1;
		// the kernel does not handle layouts
		// TODO : handle it
	}
	return ret;
}

int libinput_get_info(int fd, struct input_info *info) {
	return ioctl(fd, I_INPUT_GET_INFO, info);
}

const char *libinput_class_string(unsigned long class) {
	switch (class) {
	case IE_CLASS_KEYBOARD:
		return "keyboard";
	case IE_CLASS_MOUSE:
		return "mouse";
	case IE_CLASS_JOYSTICK:
		return "joystick";
	case IE_CLASS_UNKNOW:
	default:
		return "unknow";
	}
}


const char *libinput_subclass_string(unsigned long class, unsigned long subclass) {
	switch (class) {
	case IE_CLASS_KEYBOARD:
		switch (subclass) {
		case IE_SUBCLASS_PS2_KBD:
			return "PS2 keyboard";
		case IE_SUBCLASS_USB_KBD:
			return "USB keyboard";
		case IE_SUBCALSS_VIRT_KBD:
			return "virtual keyboard";
		case IE_SUBCLASS_UNKNOW:
		default:
			return "unknow keyboard";
		}
	case IE_CLASS_MOUSE:
		switch (subclass) {
		case IE_SUBCLASS_PS2_MOUSE:
			return "PS2 mouse";
		case IE_SUBCLASS_USB_MOUSE:
			return "USB mouse";
		case IE_SUBCLASS_VIRT_MOUSE:
			return "virtual mouse";
		case IE_SUBCLASS_PS2_TRACKPAD:
			return "PS2 trackpad";
		case IE_SUBCLASS_UNKNOW:
		default:
			return "unknow mouse";
		}
	case IE_CLASS_JOYSTICK:
		return "unknow joystick";
	case IE_CLASS_UNKNOW:
	default:
		return "unknow";
	}
}