#!/usr/bin/env sh
set -eu

# wali does this but lets ensure time is accurate before we start
timedatectl

# NOTE version must be the same as the Git tag
VERSION=v1.21
INSTALL_DIR=/root/wali
START_SH="start.sh <ip_address> [port]"
FILENAME=wali-bin_$VERSION.tar.gz

# Download tar from GitHub
curl -OL https://github.com/ccooper1982/wali/releases/download/$VERSION/$FILENAME

# Extract
tar -xf $FILENAME -C /root

echo
echo "--------------"
echo Change directory to $INSTALL_DIR
echo Then ./$START_SH
echo Default port is 8080
echo "--------------"
