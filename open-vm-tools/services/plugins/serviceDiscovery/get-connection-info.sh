#!/bin/sh

####################################################################
# Copyright (c) 2020-2024 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
####################################################################

# check if necesary commands exist
command -v ss >/dev/null 2>&1 || { echo >&2 "ss doesn't exist"; exit 1; }
command -v grep >/dev/null 2>&1 || { echo >&2 "grep doesn't exist"; exit 1; }
command -v sort >/dev/null 2>&1 || { echo >&2 "sort doesn't exist"; exit 1; }
command -v tr >/dev/null 2>&1 || { echo >&2 "tr doesn't exist"; exit 1; }

# get pids of listening processes
space_separated_pids=$(ss -lntup | grep -Eo "pid=[0-9]+" | sort -u)

# grep pattern
pattern=$(echo $space_separated_pids | tr ' ' '|')

# get matching lines
ss -antup | grep -E $pattern  | sed 's/[ \t]*$//'
