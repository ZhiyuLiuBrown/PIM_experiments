#!/bin/bash 

max_num_threads=29
num_threads=1

while [ $num_threads -lt $max_num_threads ]; do
	./test_intel64 $num_threads
	let num_threads=num_threads+3
done
