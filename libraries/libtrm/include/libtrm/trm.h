#ifndef _LIB_TRM_H
#define _LIB_TRM_H

#include <stddef.h>
#include <stdint.h>

typedef struct trm_fb {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t modifier; // for non linear
    uint32_t pitch;
    uint32_t id;
    uint32_t fd;       // the fd for this framebuffer only used for modesetting
} trm_fb_t;

typedef struct trm_plane {
    trm_fb_t *fb;   // just a helper field
    uint32_t fb_id; // actual fd id (used for mode setting)
    uint32_t possible_crtcs;
    uint32_t crtc;
    uint32_t id;
    uint32_t type;
    uint32_t src_x;
    uint32_t src_y;
    uint32_t src_w;
    uint32_t src_h;
    uint32_t dest_x;
    uint32_t dest_y;
    uint32_t dest_w;
    uint32_t dest_h;
} trm_plane_t;

#define TRM_PLANE_PRIMARY 0
#define TRM_PLANE_OVERLAY 1
#define TRM_PLANE_CURSOR  2

typedef struct trm_timings {
    uint32_t hdisplay;
    uint32_t vdisplay;
    uint32_t htotal;
    uint32_t vtotal;
    uint32_t hsync_start;
    uint32_t hsync_end;
    uint32_t vsync_start;
    uint32_t vsync_end;
    uint32_t pixel_clock;
    uint32_t refresh; // refresh rate
} trm_timings_t;

typedef struct trm_crtc {
    trm_timings_t *timings;
    uint32_t id;
    int active;
} trm_crtc_t;

typedef struct trm_connector {
    uint32_t id;
    uint32_t type;
    uint32_t possible_crtcs;
    uint32_t crtc;
    uint16_t dpms_state;
} trm_connector_t;

typedef struct trm_card {
    char name[64];
    char driver[64];
    trm_crtc_t *crtcs;
    trm_plane_t *planes;
    trm_connector_t *connectors;
    size_t crtcs_count;
    size_t planes_count;
    size_t connectors_count;
    size_t vram_size;
} trm_card_t;

typedef struct trm_palette {
    int stub;
} trm_palette_t;

typedef struct trm_mode {
    trm_palette_t *palette;
    trm_crtc_t *crtcs;
    trm_plane_t *planes;
    trm_connector_t *connectors;
    size_t crtcs_count;
    size_t planes_count;
    size_t connectors_count;
} trm_mode_t;

#define TRM_CRTC_MASK(id) (1 << (id - 1))

// IOCTLS
#define TRM_GET_RESSOURCES    7000
#define TRM_GET_MODE          7001
#define TRM_GET_FRAMEBUFFER   7002
#define TRM_GET_PLANE         7003
#define TRM_GET_CRTC          7004
#define TRM_GET_CONNECTOR     7005
#define TRM_KMS_TEST          7006
#define TRM_KMS_FIX           7007
#define TRM_KMS_COMMIT        7008
#define TRM_ALLOC_FRAMEBUFFER 7009


#endif
