---
title: virtual consoles
---
In Stanix, a **virtual console** is an object attached to a framebuffer and a keyboard, **virtual consoles** can also be attached to a **virtual terminal**.  
A **virtual terminal** is a special tty device that can be attached to a **virtual console** to displai it's content or take input.  
**Virtual terminals** (/dev/vtXX) and **virtual consoles** can be created from the **virtual console master** (/dev/vt).

## ioctls
> [!NOTE]  
> When doing a ioctl on a **virtual terminal** instead of the **virtual console master**, a lot of the time the operation can only be done on the **virtual terminal** used for this ioctl or it's attached **virtual console** and a id of 0 will refere to the **virtual terminal**'s id or it's atached **virtual console** one (a id of 0 in a ioctl passed to a **virtual console master** is not valid).

- `VT_CREATE`
  Create a new **virtual terminal**.
- `VT_DESTROY`
  Detach a **virtual terminal** from any **virtual console** and destroy it.
- `VT_SWITCH` / `VC_SWITCH`
  Switch the attached **virtual terminal** of a **virtual console** to the specified one.
- `VT_GET_VC`
  Get the id of the **virtual console** attached to a **virtual terminal**.

- `VC_CREATE`
  Create a new **virtual console**.
- `VC_DESTROY`
  Detach a **virtual console** from any framebuffer, keyboard or **virtual terminal** and destroy it.
- `VC_ATTACH_FB`
  Attach/detach a **virtual console** to/from a framebuffer.
- `VC_ATTACH_KB`
  Attach/detach a **virtual console** to/from a keyboard.
- `VC_GET_FB`
  Get the framebuffer attached to a **virtual console**.
- `VC_GET_KB`
  Get the keyboard attached to a **virtual console**.
