#!/bin/bash

CC="gcc"
CFLAGS="-Wall -Wextra -Wpedantic -g"
SRC="src/main.c src/sword.c"
LIB_SRC=kovsh/src/*.c
LIB_INCLUDE=kovsh/src

$CC $CFLAGS -o sword $SRC $LIB_SRC -I$LIB_INCLUDE
