#!/bin/bash

for i in {1..100}; do
    echo "Inserting $i ..."
    echo "add $i" >> /proc/modlist
    sleep 0.5
done