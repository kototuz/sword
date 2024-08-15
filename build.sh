#!/bin/sh

CC="gcc"
CFLAGS="-Wall -Wextra -Wpedantic -std=c17 -g"
SRC="sword.c"
LIB_SRC=kovsh/kovsh.c
LIB_INCLUDE=kovsh

mkdir -p repos.d

$CC $CFLAGS -o sword.out $SRC $LIB_SRC -I$LIB_INCLUDE

print_and_execute() {
    eval $2="$($1)"
    printf "TEST: %-44s" "$1"
}


if [ "$1" = "test" ]; then
    rm -f ./repos.d/*



    # new repo
    print_and_execute "./sword.out repo new +n test" return
    if [ -f ./repos.d/test ] && [ "$(sed '1q;d' ./repos.d/test)" = "0" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # new card
    print_and_execute "./sword.out card new +r test +l yes +t da" return
    if [ "$(sed '2q;d' ./repos.d/test)" = "yes=da" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # dump repo
    print_and_execute "./sword.out repo dump +n test" return
    if [ "$return" = "0" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # del card
    print_and_execute "./sword.out card del +r test +l yes" return
    if [ ! "$(sed '1q;d' ./repos.d/test)" = "yes=da" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # display all repos
    print_and_execute "./sword.out repo list" return
    if [ "$return" = "test" ]; then
        echo "OK"
    else
        echo "FAILED"
    fi

    # del repo
    print_and_execute "./sword.out repo del +n test" return
    if [ ! -f ./repos.d/test ]; then
        echo "OK"
    else
        echo "FAILED"
    fi


    rm -f ./repos.d/*
elif [ "$1" = "test-preset" ]; then
    rm -f ./repos.d/*
    ./sword.out repo new +n test
    ./sword.out card new +r test +l yes +t da
    ./sword.out card new +r test +l no +t net
    ./sword.out card new +r test +l here +t tut
    ./sword.out card new +r test +l train +t poezd
fi
