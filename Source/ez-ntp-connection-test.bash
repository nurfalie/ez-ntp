#!/bin/bash
# A test script.

for i in `seq 1 100`;
do
    nc -d 127.0.0.1 50000 1>/dev/null &
done
