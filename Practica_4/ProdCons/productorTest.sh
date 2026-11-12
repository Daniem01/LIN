#!/bin/bash

for i in {1..10}
do
    echo "Insertando $i"
    echo $i >> /dev/prodCons
    sleep 0.4
done