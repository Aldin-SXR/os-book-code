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

# WSL2 runs Linux in a small virtual machine of its own, so ps sees only the
# few dozen processes inside it, not the hundreds Windows is running.
if grep -qi microsoft /proc/version 2>/dev/null; then
    echo "(WSL2 counts only its own Linux processes; Task Manager's"
    echo " Performance tab shows how many Windows is running)"
fi
