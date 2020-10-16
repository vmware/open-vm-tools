#!/bin/sh

# Copyright (C) 2020 VMware, Inc.  All rights reserved.

# check if necesary commands exist
command -v ss >/dev/null 2>&1 || { echo >&2 "ss doesn't exist"; exit 1; }
command -v grep >/dev/null 2>&1 || { echo >&2 "grep doesn't exist"; exit 1; }
command -v sort >/dev/null 2>&1 || { echo >&2 "sort doesn't exist"; exit 1; }
command -v tr >/dev/null 2>&1 || { echo >&2 "tr doesn't exist"; exit 1; }
command -v ps >/dev/null 2>&1 || { echo >&2 "ps doesn't exist"; exit 1; }

# get pids of listening processes
space_separated_pids=$(ss -lntup | grep -Eo "pid=[0-9]+" | grep -Eo "[0-9]*" | sort -u)

# ps accepts comma separated pids
comma_separated_pids=$(echo $space_separated_pids | tr ' ' ',')

# get information for each listening process
ps --pid $comma_separated_pids -o pid=,ppid=,comm=,command=
