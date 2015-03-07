#!/bin/bash

function usage() {
    printf "usage: test.sh COUNT\n\n"
    printf "Start a program pms. An example implementation of Pipeline Merge Sort algorithm.\n"
    printf "To run the program it is necessary to have OpenMPI installed.\n\n"
    printf "Options:\n"
    printf "  COUNT - count of random numbers generated as an input for pms (should be power\n"
    printf "          of 2, otherwise it will be rounded to next bigger power of 2)\n"
}

if [ "$#" != "1" ]; then
    usage
    exit 1
fi
