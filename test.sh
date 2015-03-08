#!/bin/bash

usage() {
    printf "usage: test.sh COUNT\n\n"
    printf "Start a program pms. An example implementation of Pipeline Merge Sort algorithm.\n"
    printf "To run the program it is necessary to have OpenMPI installed.\n\n"
    printf "Options:\n"
    printf "  COUNT - count of random numbers generated as an input for pms (should be power\n"
    printf "          of 2, otherwise it will be rounded to next bigger power of 2)\n"
}

generate_numbers() {
    local source="/dev/urandom"
    local filename="numbers"

    if [ -z "$1" ]; then
        echo "generate_numbers requires exactly one argument" 1>&2
        exit 1
    fi

    rm -f "$filename" >/dev/null 2>&1

    dd if="$source" of="$filename" bs=1 count="$1" >/dev/null 2>&1
}

is_pow_2() {
    declare -i r

    if [ -z "$1" ]; then
        echo "is_pow_2 requires exactly one argument" 1>&2
        exit 1
    fi

    r=$(echo "is_pow_2($1)" | bc -lq pow2.bc 2>/dev/null)
    return $r
}

next_pow_2() {
    declare -i r

    if [ -z "$1" ]; then
        echo "next_pow_2 requires exactly one argument" 1>&2
        exit 1
    fi

    r=$(echo "next_pow_2($1)" | bc -lq pow2.bc 2>/dev/null)
    echo "$r"
}

if [ -z "$1" ]; then
    usage
    exit 1
fi

count="$1"
is_pow_2 "$count"

if [ "$?" != "1" ]; then
    count=$(next_pow_2 "$count")
fi

generate_numbers "$count"

make -s
