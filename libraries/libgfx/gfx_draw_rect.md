---
title: gfx_draw_rect
---
```c
void gfx_draw_rect(gfx_t *gfx,color_t color,long x,long y,long width,long height);
```

## use
`gfx_draw_rect` draw a rectangle in the backbuffer of the [`gfx_t`](gfx_t) context `gfx` of color `color` at `x`;`y` of width `width` and height `height`.

## note
In the future the coordinate might become unsigned.
