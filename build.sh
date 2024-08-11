#!/bin/bash

CC="gcc"
CFLAGS="-Wall -Wextra -Wpedantic -g"
SRC="sword.c"
LIB_SRC=kovsh/kovsh.c
LIB_INCLUDE=kovsh

mkdir -p repos.d

$CC $CFLAGS -o sword.out $SRC $LIB_SRC -I$LIB_INCLUDE


function print_and_execute() {
    eval $2=$($1)
    printf "TEST: %-44s" "$1"
}


if [[ $1 = "test" ]]; then
    rm -f ./repos.d/*



    # new repo
    print_and_execute "./sword.out new repo +n test" return
    if [ -f ./repos.d/test ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # new card
    print_and_execute "./sword.out new card +r test +l yes +t da" return
    if [ "$(sed '1q;d' ./repos.d/test)" = "yes=da" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # del card
    print_and_execute "./sword.out del card +r test +l yes" return
    if [ ! "$(sed '1q;d' ./repos.d/test)" = "yes=da" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # display all repos
    print_and_execute "./sword.out ls" return
    if [ "$return" = "test" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # del repo
    print_and_execute "./sword.out del repo +n test" return
    if [ ! -f ./repos.d/test ]; then
        echo "OK"
    else
        echo "FAILED"
    fi


    rm -f ./repos.d/*
fi
