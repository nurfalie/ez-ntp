#!/bin/bash
# A test script.

for i in `seq 1 10`;
do
    nc -dv zebra 50000 1>/dev/null &
done
