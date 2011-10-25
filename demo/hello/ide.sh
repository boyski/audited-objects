#!/bin/sh

for i in 1 2; do
    echo "Build #$i:"
    gmake clean all
    sleep 1
done
