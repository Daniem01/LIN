#!/bin/bash

while true; do
    for i in {200..299}; do
        echo "add $i" >> /proc/modlist
        sleep 1
    done
    echo "cleanup" >> /proc/modlist
done