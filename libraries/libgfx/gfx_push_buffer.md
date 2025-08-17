---
title: gfx_push_buffer
---
```c
void gfx_push_buffer(gfx_t *gfx);
```

## use
`gfx_push_buffer` push content of the backbuffer to the framebuffer.

## note
By default when an application draw to a [`gfx_t`](gfx_t) context it do not get draw direcly into the framebuffer, but only in the backbuffer. Application should use `gfx_push_buffer` or `gfx_push_backbuffer` to push the back buffer to frontbuffer(framebuffer)
