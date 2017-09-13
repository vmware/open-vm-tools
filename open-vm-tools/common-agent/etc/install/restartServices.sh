#!/bin/sh

dir=$(cd -P -- "$(dirname -- "$0")" && pwd -P)

. "$dir"/commonenv.sh

#Stop configured services
for cafService in $cafServices; do 
    /sbin/chkconfig $cafService
    if [ $? -eq 0 ]; then
        /sbin/service $cafService restart
    fi
done
