---
title: gfx_open_framebuffer
---
```c
gfx_t *gfx_open_framebuffer(const char *path);
```

## use
`gfx_open_framebuffer` open a new [`gfx_t`](gfx_t) context for the specified framebuffer.    
For portable behaviour applications should pass NULL as path (this will use the value from the `FB` environement variable).

## return value
`gfx_open_framebuffer` return a pointer to a new [`gfx_t`](gfx_t) context or a NULL pointer and set errno in case of error (eg : no framebuffer found, ...)

## error
- `ENOENT`
  The specified framebuffer don't exist.
- `EINVAL`
  `path` is not a framebuffer.
