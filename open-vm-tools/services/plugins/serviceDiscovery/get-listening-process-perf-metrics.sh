#!/bin/sh

# Copyright (C) 2020 VMware, Inc.  All rights reserved.

# check if necesary commands exist
command -v ss >/dev/null 2>&1 || { echo >&2 "ss doesn't exist"; exit 1; }
command -v grep >/dev/null 2>&1 || { echo >&2 "grep doesn't exist"; exit 1; }
command -v sort >/dev/null 2>&1 || { echo >&2 "sort doesn't exist"; exit 1; }
command -v awk >/dev/null 2>&1 || { echo >&2 "awk doesn't exist"; exit 1; }
command -v cat >/dev/null 2>&1 || { echo >&2 "cat doesn't exist"; exit 1; }
command -v cut >/dev/null 2>&1 || { echo >&2 "cut doesn't exist"; exit 1; }
command -v pgrep >/dev/null 2>&1 || { echo >&2 "pgrep doesn't exist"; exit 1; }
command -v getconf >/dev/null 2>&1 || { echo >&2 "getconf doesn't exist"; exit 1; }

ECHO="$(which echo)"

if [ "$ECHO" = "" ]; then
  ECHO=echo
fi

calculateCpuTimeForProcess() {
  pidArray="$@"
  cpuUsage=0
  for i in $pidArray; do
    pidStatArray=$(cat /proc/$i/stat)
    uptimes=$(cat /proc/uptime)
    uptime=$($ECHO ${uptimes} | cut -d' ' -f1)
    utime=$($ECHO ${pidStatArray} | cut -d' ' -f14)
    stime=$($ECHO ${pidStatArray} | cut -d' ' -f15)
    cutime=$($ECHO ${pidStatArray} | cut -d' ' -f16)
    cstime=$($ECHO ${pidStatArray} | cut -d' ' -f17)
    starttime=$($ECHO ${pidStatArray} | cut -d' ' -f22)
    pidCPU=$($ECHO - | awk -v ut=$utime -v st=$stime -v cut=$cutime -v cst=$cstime -v start=$starttime -v clk=$CLK_TCK -v up=$uptime '{print (ut+st+cut+cst)}')
    cpuUsage=$($ECHO - | awk -v ti=$timeout -v clk=$CLK_TCK -v cpu=$cpuUsage -v pcpu=$pidCPU -v ccpu=$PROC_COUNT '{print (((cpu+pcpu)/(ti*clk)*100)/ccpu)}')
  done
  $ECHO $cpuUsage
}

calculateMemForAllProcesses() {
  pids=$@
  for k in $pids; do
    pidArray="$k $(pgrep -P $k)"
    length=$($ECHO -n $pidArray | wc -w)
    PGPID=$k
    if [ $length -eq 1 ]; then
      memUsage=$(awk '/^Pss:/{A+=$2} END {print A}' $(for n in $($ECHO "$PGPID" | awk '{if (NR > 1 || $n!="") print "/proc/"$n"/smaps"}'); do $ECHO $n ; done) || exit 1)
    else
      memUsage=$(awk '/^Pss:/{A+=$2} END {print A}' $(for n in $($ECHO -e "\n$PGPID\n$(pgrep -P$PGPID)" | awk '{if (NR > 1 || $n !="") print "/proc/"$n"/smaps"}'); do $ECHO $n ; done) || exit 1)
    fi
    $ECHO MEM: $k  $memUsage
  done
}

calculateIOForAllProcesses() {
  pids=$@
  for l in $pids; do
    pidArray="$l $(pgrep -P $l)"
    length=$($ECHO -n $pidArray | wc -w)
    PGPID=$l
    if [ $length -eq 1 ]; then
      readBytes=$(awk '/^read_bytes:/{A+=$2} END {print A}' $(for n in $($ECHO "$PGPID" | awk '{if (NR > 1 || $n!="") print "/proc/"$n"/io"}'); do $ECHO "$n" ; done) || exit 1)
      writeBytes=$(awk '/^write_bytes:/{A+=$2} END {print A}' $(for m in $($ECHO "$PGPID" | awk '{if (NR > 1 || $m!="") print "/proc/"$m"/io"}'); do $ECHO "$m" ; done) || exit 1)
    else
      readBytes=$(awk '/^read_bytes:/{A+=$2} END {print A}' $(for n in $($ECHO -e "\n$PGPID\n$(pgrep -P$PGPID)" | awk '{if (NR > 1 || $n !="") print "/proc/"$n"/io"}'); do $ECHO $n ; done) || exit 1)
      writeBytes=$(awk '/^write_bytes:/{A+=$2} END {print A}' $(for m in $($ECHO -e "\n$PGPID\n$(pgrep -P$PGPID)" | awk '{if (NR > 1 || $m !="") print "/proc/"$m"/io"}'); do $ECHO $m ; done) || exit 1)
    fi
    $ECHO IO: $l $readBytes $writeBytes
  done
}

calculateCpuForAllProcesses() {
  pids=$@
  for j in $pids; do
    pidArray="$j $(pgrep -P $j)"
    cpuUsage=$(calculateCpuTimeForProcess $pidArray)
    $ECHO CPU: $j $cpuUsage
  done
}

run() {
  calculateCpuForAllProcesses $@

  calculateIOForAllProcesses $@

  sleep $timeout

  # need to get CPU and IO for two different timestamps to calculate delta
  calculateCpuForAllProcesses $@

  calculateIOForAllProcesses $@

  calculateMemForAllProcesses $@
}

get_performance_metrics() {
  if [ $# -lt 1 ]; then
    echo "No process id has been provided."
    exit 1
  fi

  pids=$@
  echo "#PIDs: - $pids"

  timeout=1
  CLK_TCK=$(getconf CLK_TCK)
  [ -z "$CLK_TCK" ] && { echo "Failed to get conf variable CLK_TCK"; exit 1; }
  PROC_COUNT=$(getconf _NPROCESSORS_ONLN)
  [ -z "$PROC_COUNT" ] && { echo "Failed to get conf variable _NPROCESSORS_ONLN"; exit 1; }

  run $pids
}

space_separated_pids=$(ss -lntup | grep -Eo "pid=[0-9]+" | grep -Eo "[0-9]+" | sort -u)

get_performance_metrics $space_separated_pids