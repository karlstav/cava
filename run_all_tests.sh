#!/bin/bash
set -e
## run these commands before testing:
# squeezelite -v -m 51:fb:32:f8:e6:9f -z
# sudo modprobe snd-aloop
# arecord -D hw:Loopback,1 -c 2 -r 44100 > /tmp/fifo &
TESTCFGS="example_files/test_configs/*"
ERRFILE=$(mktemp)
trap 'rm -f "$ERRFILE"' EXIT
CAVA_BIN=${CAVA_BIN:-./cava}
for f in $TESTCFGS
do
    echo "testing $f"

    method=$(awk -F= '
        /^\[input\]/{in_input=1; next}
        /^\[/{in_input=0}
        in_input && $1 ~ /^[[:space:]]*method[[:space:]]*$/ {gsub(/[[:space:]]/,"",$2); print $2; exit}
    ' "$f")
    if [ "$method" = "shmem" ]; then
        source=$(awk -F= '
            /^\[input\]/{in_input=1; next}
            /^\[/{in_input=0}
            in_input && $1 ~ /^[[:space:]]*source[[:space:]]*$/ {gsub(/^[[:space:]]+|[[:space:]]+$/,"",$2); print $2; exit}
        ' "$f")
        shm_name="${source#/}"
        if [ ! -e "/dev/shm/$shm_name" ]; then
            echo "SKIP (missing /dev/shm/$shm_name)"
            continue
        fi
    fi

    if [ "$method" = "sndio" ]; then
        if [ ! -S "/tmp/sndio.sock" ] && [ ! -S "/tmp/sndio/sock" ] && [ ! -S "/var/run/sndio/sock" ]; then
            echo "SKIP (sndio socket not found)"
            continue
        fi
    fi

    if ! "$CAVA_BIN" -p "$f" > /dev/null 2>"$ERRFILE"; then
        cat "$ERRFILE" >&2
        exit 1
    fi
    echo "OK!"
done
echo "all tests completed successfully"
## clean up:
# killall squeezelite
# sudo rmmod snd_aloop
# killall arecord
exit 0
