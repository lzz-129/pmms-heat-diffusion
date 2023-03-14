#!/bin/bash
IMG=$HOME/pmms-heat-diffusion/images/pat1_"$2"x"$3".pgm
ls $IMG
for i in {1..32}
do
   "$1" -i $IMG -m "$2" -n "$3" -p "$i" | grep Histo |awk '{print($3)}'
done

