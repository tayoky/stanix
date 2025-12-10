---
title: login
---

## description
**login** will start a new session, display `/motd`, initialize environement variables and launch the shell.

## options
- **-f**
  Bypass login and stay loge as the current user (used for auto login).
- **--setup-stdin-from-stdout**
  Hack option used in the old days when tash didn't exist and tsh could only setup stdout, this option execute a dup2 from stdout to stdin.

## exit status
Return 0 if no error happend else return 1.
