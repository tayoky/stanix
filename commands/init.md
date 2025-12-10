---
title: init
---
## description
**init** is the first program launched by the kernel.  
Its role is to init standards streams and launch the **startup script** (`/startup.sh`).

## notes
For security reason **init** will refuse to start if its pid isen't 0 (it's not run as the first program).
