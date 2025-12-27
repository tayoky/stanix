---
title: trm
---
**TRM** is the graphics rendring manager and KMS system of stanix, inspired by DRM. 

## opening
To start, the application open the TRM device (`/dev/video0`, ...) to get a gpu fd, the first process to open the device get a special fd called the **master**.

## master
The **master** is a special gpu fd that has special permission such has being able to do **modesetting**, opening and mapping any framebuffer or droping privelege.

### getting a master
There are two ways to get a master :
- a process open a TRM device and nodody has open it
- a process claim master using a special TRM operation (require nobody to be master or being root)

## card
A card define all settings of a gpu. It is described as follows :
```c
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
```

## modes
A mode is a collection of settings that can be filed or empty.
```c
typedef struct trm_mode {
    trm_palette_t *palette;
    trm_crtc_t *crtcs;
    trm_plane_t *planes;
    trm_connector_t *connectors;
    size_t crtcs_count;
    size_t planes_count;
    size_t connectors_count;
} trm_mode_t;
```
If a field is keep to NULL or not provided it stay the same as the current one.

### KMS(modesetting)
Unlike DRM, TRM's KMS uses a "test-fix-commit" api, with three phases :  
- test
  ask the **TRM** driver to see if the gpu/screen can **support** the specified mode.
- fix
  ask the **TRM** driver to **fix** the specified mode to work on the gpu/screen.
- commit
  ask the **TRM** driver to **commit** the specified mode atomicly (with minimum flicker in a single retrace)

> [!NOTE]
> During the fix phase the corrected mode can be far from the specified one, application should check if the mode is close enought before commiting.

## plane
A **plane** is memory region that contain graphics data, a plane must be attached to a framebuffer to be used. It is described as follows :
```c
typedef struct trm_plane {
    trm_fb_t *fb;
    uint32_t fb_id;
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
```

> [!NOTE] 
> note on primary planes  
> The **primary planes** must have the lowest `id`, a type of `TRM_PLANE_PRIMARY`, a `dest_x` and `dest_y` of 0 and a `dest_w` and `dest_y` equal to the `hdisplay` and `vdisplay` of it's crtc.

> [!NOTE]
> The number of plane is fixed and cannot chabges, planes have a continious `id` (e.g. if there are two plane they will have id 0 and 1)

## framebuffers
A **framebuffer** describe the data of a plane. It is saved as follows :  
```c
typedef struct trm_fb {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t modifier; // for non linear
    uint32_t pitch;
    uint32_t id;
    uint32_t fd;       // the fd for this framebuffer only used for modesetting
} trm_fb_t;
```

### framebuffer operations
Three operations can be done on **framebuffers** they can be allocated, mapped or freed.
> [!NOTE] a fourth operation (opening framebuffer) can be done if the gpu fd is the master. It allows to get the framebuffer's fd using it's ID.

## crtc
The **crtc** or **CRT controller** describe all setrings of the crtc. It is described as follows :
```c
typedef struct trm_crtc {
    trm_timings_t *timings;
    uint32_t id;
    int active;
} trm_crtc_t;
```

### crtc timings
The **crtc timings** describe all the timings used by the crtc such as the **refresh rate** or the **pixel clock**. It is described as follows :
```c
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
```

## connector
A **connector** describe a physical connection with a monitor. It is described as follows :
```c
typedef struct trm_connector {
    uint32_t id;
    uint32_t type;
    uint32_t possible_crtcs;
    uint32_t crtc;
    uint16_t dpms_state;
} trm_connector_t;
```
