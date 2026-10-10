#!/bin/sh
# crowd.sh -- how many programs are running, and how many processors there are
# to run them on.
if [ "$(uname)" = "Darwin" ]; then
    cores=$(sysctl -n hw.ncpu)
else
    cores=$(nproc)
fi
procs=$(ps ax | wc -l)
procs=$((procs - 1))            # the header line is not a process
echo "processes running: $procs"
echo "processors:        $cores"
echo "ratio:             $((procs / cores)) programs per processor"
