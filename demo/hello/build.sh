#!/bin/sh

set -x
gcc -c hello.c
gcc -c goodbye.c
gcc -o hello.exe hello.o goodbye.o
