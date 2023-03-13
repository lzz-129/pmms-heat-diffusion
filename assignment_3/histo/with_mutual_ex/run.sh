#!/bin/bash
for i in {1..32}
do
   $1 ../../../images/pat1_100x150.pgm -n 150 -m 100 -p "$i"
done
