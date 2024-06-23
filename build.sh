#!/bin/bash

CC="gcc"
CFLAGS="-Wall -Wextra -Wpedantic -g"
SRC="src/main.c src/sword.c src/utils.c"
LIB_SRC=kovsh/src/*.c
LIB_INCLUDE=kovsh/src

$CC $CFLAGS -o sword.out $SRC $LIB_SRC -I$LIB_INCLUDE
