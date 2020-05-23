#!/bin/bash
i=0;
while [ $i -lt 100 ]; do
	echo $i
	./yatka&
	sleep 0.3
	kill -INT $!
	i=$[i+1]
done;