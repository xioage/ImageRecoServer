#!/bin/bash

for i in {1..10}
do
    gnome-terminal -x bash -c './gpu_fv l 5;bash' &
done
