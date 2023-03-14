#!/bin/bash
IMG=$HOME/pmms-heat-diffusion/images/gradient_"$2"x"$3".pgm
ls $IMG
for i in histo_atomic histo_mutex histo_semaphores histo_sw_transactional
do
   echo $i
   for j in {1..16}
   do
      ./$i/$i -i $IMG -m "$2" -n "$3" -p $j | grep Histo |awk '{print($3)}'
   done
done

