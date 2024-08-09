#!/bin/bash

CC="gcc"
CFLAGS="-Wall -Wextra -Wpedantic -g"
SRC="sword.c"
LIB_SRC=kovsh/kovsh.c
LIB_INCLUDE=kovsh

$CC $CFLAGS -o sword.out $SRC $LIB_SRC -I$LIB_INCLUDE
