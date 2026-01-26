#!/usr/bin/env sh
set -eu

# Download tar from GitHub
VERSION=v0.8
INSTALL_DIR=/usr/local/bin/wali
START_SH="start.sh <ip_address> [port]"

curl -OL https://github.com/ccooper1982/wali/releases/download/$VERSION/wali-bin_$VERSION.tar.gz

# Extract
tar -xf wali-bin_$VERSION.tar.gz -C /usr/local/bin

# wali does this but lets ensure time is accurate before we start
timedatectl

echo
echo "--------------"
echo Change directory to $INSTALL_DIR
echo Then ./$START_SH
echo "--------------"
