#!/bin/sh

# sudo ip address add 192.168.1.1/24 broadcast + dev enp3s0f3u2
# ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@192.168.1.2
# mount -o remount,size=600M /run/archiso/cowspace

pkill wali

# TODO copy resources to build/wwwroot

PORT=8080

if [ -z "$1" ]
  then
    echo "No IP address set"
    exit 1
fi

if [ -n "$2" ]
  then
  $PORT=$2
fi

echo "Using $1:$PORT"

./wali  --config wwwroot/wt_config.xml\
        --docroot "wwwroot"\
        --http-address $1\
        --http-port $PORT\
        -t -1;
