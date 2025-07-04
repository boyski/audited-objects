#!/bin/sh

for i in $(seq 3); do
    echo "Build #$i:"
    make clean all
    sleep 1
done
