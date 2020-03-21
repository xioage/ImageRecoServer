#!/bin/bash

for i in {1..45}
do
    #gnome-terminal -x bash -c './gpu_fv s l 5 51717 5;bash' &
    ./gpu_fv s l 5 51717 5 &
done
