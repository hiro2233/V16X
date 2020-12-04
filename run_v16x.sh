#!/bin/sh

argc="$#"

if [ $argc -lt 4 ] ; then
    echo "Usage: $0 [-h] [-a <addr>] [-p <port>]
       -a <address>   - Bind to IP address
       -p <port>      - Bind to port"
    exit 0
fi

cd htdocs
sh -c "sleep 1 && xdg-open http://$2:$4/static" &

../build/release/v16x $1 $2 $3 $4
