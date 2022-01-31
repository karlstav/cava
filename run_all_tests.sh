#!/bin/bash
set -e

## run these commands before testing:
# squeezelite -v -m 51:fb:32:f8:e6:9f -z
# sudo modprobe snd-aloop
# arecord -D hw:Loopback,1 -c 2 -r 44100 > /tmp/fifo &

TESTCFGS="example_files/test_configs/*"
for f in $TESTCFGS
do
    echo "testing $f"
    ./cava -p $f > /dev/null
    echo "OK!"
done
echo "all tets completed successfully"

## clean up:
# killall squeezelite
# sudo rmmod snd_aloop
# killall arecord
exit 0