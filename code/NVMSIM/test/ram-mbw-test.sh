#!/bin/bash
i=0;
while [[ $i -lt $1 ]]
do
    echo to cpu$i
    taskset -c $i mbw -q -n $2 $3 > ./config/ram-test &
    ((i++));
done