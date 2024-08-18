#!/usr/bin/bash

# portsinuse=$(netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
#         | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
#         | grep -E "[0-9]+" | uniq | tr "\n" " ")

portsinuse=$(netstat -nat \
        | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
        | grep -E "[0-9]+" | uniq | tr "\n" " ")
echo "Ports in use: $portsinuse"

trap "echo 'Alarm signal received!'; " ALRM

# Set an alarm for 5 seconds
sleep 5 &
wait $!

# Manually send SIGALRM to this script
kill -ALRM $$

echo hello