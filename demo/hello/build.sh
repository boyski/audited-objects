#!/bin/sh

set -x
gcc -c hello.c
gcc -o hello.exe hello.o
