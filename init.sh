#!/bin/bash

#build tlibc
make -C tlibc

#then install it
cd ./tlibc && ./install.sh ../userspace