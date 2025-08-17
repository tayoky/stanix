---
title: gfx_t
---
```c
typedef struct gfx_context {
    void *framebuffer;
    void *backbuffer;
    void *buffer;
    long width;
    long height;
    long pitch;
    int bpp;
    int red_mask_shift;
    int red_mask_size;
    int green_mask_shift;
    int green_mask_size;
    int blue_mask_shift;
    int blue_mask_size;
} gfx_t;
```

## use
`gfx_t` represent a gfx context.  

# note
Application should only use the `width` and `height` field. Other fields are for internallibgfx use.
Application should __never__ modify any vakue of a `gfx_t`.
