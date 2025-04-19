---
title: kernel modules
---
**kernel modules** are runtime loadable object files  
they are placed in the `/mod` folder of the **initial ramdisk** and **sysroot**

## creating a kernel module
creating a kernel module is simple just add your c source file into the `modules` of the main stanix repo and run `make`

## writing a kernel module
the most simple kernel module is 
```c
#include <kernel/module.h>
#include <kernel/print.h>

int init(int argc,char **argv){
    kdebugf("hello world !\n");
    return 0;
}

int fini(){
    return 0;
}

kmodule module_meta = {
    .init = init,
    .fini = fini,
    .description = "hello world",
    .name = "hello module",
}
```

on the first line we include `<kernel/module.h>` , all kernel header (except arch specific one) are accessible trought `<kernel/XXX.h>`  
`<kernel/module.h>` define some macro and the kmodule struct to define module meta data

`module_meta` all kernel module must have a kmodule struct call `module_meta` it give the module loader information about the module
- `init` : define the function called when the module is added (here the `init` function)
- `fini` : define the function called before the module is unloaded (here the `fini` function)
- `author` (optional) : define the author of the module
- `description` (optional) 
- `liscence` (optional)
- `name` : the name used to reference the module (the name showed in `/proc/modules` , ... )

> [!NOTE]
> while **kernel modules** can reference all the **core kernel** symbols if a **kernel module** don't export a symbol using the `EXPORT(sym)` macro (defined in `<kernel/modules.h>`) it is inacesible from other **kernel modules**

> [!NOTE]
> a lot of string function are avalible for kernel se   
> don't forget to include `<kernel/string.h>` instead of `<string.h>`

> [!WARNING]
> **kernel modules** must be in C , C++ won't work for the moment (it's a work in progress)

## memory allocation
memory allocation functions are avalible under `<kernel/kheap.h>`  
you can use `void * kmalloc(size_t amount)` and `void kfree(void *ptr)`  
