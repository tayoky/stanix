#include "panic.h"

#define DEF(func,...) void func (__VA_ARGS__){panic(#func,NULL);}

DEF(init_paging)
DEF(map_page)
DEF(unmap_page)
DEF(virt2phys)
DEF(space_virt2phys)
DEF(create_addr_space)
DEF(delete_addr_space)
DEF(get_addr_space)

DEF(init_serial)
DEF(write_serial_char)
DEF(write_serial)

DEF(init_timer)

DEF(jump_userspace)
DEF(context_switch,process *old,process *new)