---
title: term
---

## description
**term** is the terminal emulator of stanix and depands on **libgfx** and **libtwm**.  
Launching **term** will create a new terminal emulator with **login** open in it.

## environ
`TWM` is used to get the path to the twm socket.
`FONT` is used to know which font to use.

## exit status
Return 0 if no error happend and child exited properly.  
Return >0 if an error happend or child exited with error code.
