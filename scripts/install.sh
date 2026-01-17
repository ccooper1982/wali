#!/usr/bin/env sh
set -eu

# Download tar from GitHub
VERSION=v0.6
INSTALL_DIR=/usr/local/bin/wali
START_SH="start.sh <ip_address> [port]"

curl -OL https://github.com/ccooper1982/wali/releases/download/$VERSION/wali-bin_$VERSION.tar.gz

# Extract
tar -xf wali-bin_$VERSION.tar.gz -C /usr/local/bin

# Install
mount -o remount,size=600M /run/archiso/cowspace

pacman-key --init
pacman -Sy --noconfirm archlinux-keyring
pacman -Q wt || pacman -Sy --noconfirm wt
pacman -Q lshw || pacman -Sy --noconfirm lshw

pacman-key --init

echo
echo "--------------"
echo Change directory to $INSTALL_DIR
echo Then ./$START_SH
echo "--------------"
