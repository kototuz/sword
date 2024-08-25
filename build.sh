
CC="gcc"
CFLAGS="-Wall -Wextra -Wpedantic -std=c17 -g"
SRC="sword.c"
LIB_SRC=kovsh/kovsh.c
LIB_INCLUDE=kovsh

mkdir -p repos.d

$CC $CFLAGS -o sword.out $SRC $LIB_SRC -lncursesw -lmenuw

assert() {
    local test="$1"
    local msg="$2"

    if [ $test ]; then
        return 0
    else
        echo "FAILED: $msg"
        return 1
    fi
}

assert_eq() {
    local op1="$1"
    local op2="$2"
    local msg="$3"

    if [ "$1" = "$2" ]; then
        return 0
    else
        echo "FAILED: $msg"
        return 1
    fi
}

if [ "$1" = "test" ]; then
    rm -f ./repos.d/*



    ./sword.out repo new +n test
    assert "-f ./repos.d/test" "creating new repo"
    assert_eq "$(sed '1q;d' ./repos.d/test)" "0 0 0" "initializing new repo with defaults"

    ./sword.out card new +r test +l yes +t da
    assert_eq "$(sed '2q;d' ./repos.d/test)" "0 yes=da" "creating new flaschard"

    assert_eq "$(./sword.out repo dump +n test)" "$(echo -e "5 0 1\n0 yes=da")" "repo dumping"

    ./sword.out card del +r test +l yes
    assert_eq "" "" "deleting a flashcard"

    assert_eq "$(./sword.out repo list)" "test" "list all repos"

    ./sword.out repo del +n test
    assert "! -f ./repos.d/test"



    rm -f ./repos.d/*
elif [ "$1" = "test-preset" ]; then
    rm -f ./repos.d/*
    ./sword.out repo new +n test
    ./sword.out card new +r test +l test0 +t test0
    ./sword.out card new +r test +l test1 +t test1
    ./sword.out card new +r test +l test2 +t test2
    ./sword.out card new +r test +l test3 +t test3
elif [ "$1" = "r" ]; then
    ./sword.out $(echo "$@" | sed -e 's/\<'$1'\>//g')
fi
