---
title: libinput keyboard layouts
---
**libinput** use keyboard layout files.
In a keybaord layout file each statement can be a **statement**, a **comment**, or an enmpty line.

## statements
Two types of statements are possible, **include statements** and **assignement statements**.

## includes
`include LAYOUT` include another layout file inside the current layout file.

## assignement
`SCANCODE LOWER [UPPER] [ALT] [UPPER-ALT]` assign a symbol/key to a scancode. The choiced symbol depends on the current modifier.  
|                    | no shift modifier | shift modifier |
| ------------------ | ----------------- | -------------- |
| no alt gr modifier | LOWER             | UPPER          |
| alt gr modifier    | ALT               | UPPER-ALT      |

Scancodes can be given as decimal hexadecimal or octal.
Symbols can be given as litteral symbols such as `a` or key name such as `LSHIFT`. Valids key names are all key names macros in `<sys/input.h>` and `<input.h>`.
If no UPPER symbol is given the upper version of the LOWER symbol is used instead if it exist else the LOWER symbol is used.
If no ALT symbol is given the LOWER symbol iq used instead.
If no ALT-UPPER symbol is given the upper version of the ALT symbol is used instead if it exist else the ALT symbol is used.

A later assignement overide an older one without issues, so the following is valid.
```
0x1 h K 7
0x ESC
```

## comments
Lines that are comments start with an #.
