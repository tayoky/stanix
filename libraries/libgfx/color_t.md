---
title: color_t
---
```c
typedef uint32_t color_t;
```

## use
`color_t` represent a gfx color.

## note
`color_t` are [`gfx_t`](gfx_t) context specific, a color created for a context might not work on another context.
__Application should never use the same__ `color_r` __on multiples__ [`gfx_t`](gfx_t)__.__
