#!/bin/bash

while true; do
    for i in {100..199}; do
        echo "add $i" >> /proc/modlist
        sleep 3
    done
    echo "cleanup" >> /proc/modlist
done