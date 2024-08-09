#!/bin/bash

CC="gcc"
CFLAGS="-Wall -Wextra -Wpedantic -g"
SRC="sword.c"
LIB_SRC=kovsh/kovsh.c
LIB_INCLUDE=kovsh

mkdir -p repos.d

$CC $CFLAGS -o sword.out $SRC $LIB_SRC -I$LIB_INCLUDE
