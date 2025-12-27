---
title: TRM devices
---
TRM devices (/dev/videoXX) represent a GPU (vga, ...) and have a set of ioctl to control them. For more info see [trm](../miscellaneous/trm).

## ioctl
- `TRM_GET_RESSOURCES`
  Pass a `trm_card_t *` and fill it.
- `TRM_GET_MODE`
  Pass a `trm_mode_t *` and fill it.
- `TRM_GET_FRAMEBUFFER`
  Pass a `trm_fb_t *` and fill it (require to own the framebuffer or be the master).
- `TRM_GET_PLANE`
  Pass a `trm_plane_t *` and fill it.
- `TRM_GET_CRTC`
  Pass a `trm_crtc_t *` and fill it.
- `TRM_GET_CONNECTOR`
  Pass a `trm_connector_t *` and fill it.
- `TRM_KMS_TEST`
  Pass a `trm_mode_t *` and check it's validity.
- `TRM_KMS_FIX`
  Pass a `trm_mode_t *` and try to fix it.
- `TRM_KMS_COMMIT`
  Commit modesetting using a `trm_mode_t *`
- `TRM_ALLOC_FRAMEBUFFER`
  Pass a `trm_fb_t *` (with no id) and try to allocate it.
