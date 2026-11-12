#!/bin/bash

for i in {1..100}; do
    echo "Erasing $i ..."
    echo "remove $i" >> /proc/modlist
    cat /proc/modlist
    sleep 0.5
done