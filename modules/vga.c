#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/trm.h>
#include <module/pci.h>

static device_driver_t vga_driver;

static int vga_commit_mode(trm_gpu_t *gpu, trm_mode_t *mode) {

}


static trm_ops_t vga_ops = {

};

static int vga_check(bus_addr_t *addr) {
    pci_addr_t *pci_addr = (pci_addr_t*)addr;
    if (addr->type != BUS_PCI) return 0;
    if (pci_addr->class == 0x0 && pci_addr->subclass == 0x1) {
        return 1;
    }
    return 0;
}

static int vga_probe(bus_addr_t *addr) {
    pci_addr_t *pci_addr = (pci_addr_t*)addr;

    trm_gpu_t *gpu = kmalloc(sizeof(trm_gpu_t));
    memset(gpu, 0, sizeof(trm_gpu_t));
    strcpy(gpu->card.name, "VGA compatible controller");
    gpu->card.vram_size = 256 * 1024;
    gpu->ops = &vga_ops;
    gpu->device.driver = &vga_driver;

    // setup one primary plane
    gpu->card.planes_count = 1;
    gpu->card.planes = kmalloc(sizeof(trm_plane_t));
    gpu->card.planes[0].type           = TRM_PLANE_PRIMARY;
    gpu->card.planes[0].possible_crtcs = TRM_CRTC_MASK(1);
    gpu->card.planes[0].crtc           = 1;
    
    // setup one crtc
    gpu->card.crtcs_count = 1;
    gpu->card.crtcs = kmalloc(sizeof(trm_crtc_t));
    gpu->card.crtcs[0].active = 1;

    // setup one connector
    gpu->card.connectors_count = 1;
    gpu->card.connectors = kmalloc(sizeof(trm_connector_t));
    gpu->card.connectors[0].possible_crtcs = TRM_CRTC_MASK(1);
    gpu->card.connectors[0].crtc           = 1;

    register_trm_gpu(gpu);

    return 0;
}

static device_driver_t vga_driver = {
    .name = "vga driver",
    .check = vga_check,
    .probe = vga_probe,
};

int vga_init(int argc, char **argv){
    register_device_driver(&vga_driver);
	return 0;
}

int vga_fini(){
    unregister_device_driver(&vga_driver);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = vga_init,
	.fini = vga_fini,
	.author = "tayoky",
	.name = "vga driver",
	.description = "TRM vga driver",
	.license = "GPL 3",
};