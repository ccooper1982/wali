#!/bin/sh

# Download tar from GitHub
VERSION=v0.1-alpha1
INSTALL_DIR=/usr/local/bin/wali
START_SH="start.sh <ip_address> [port]"

curl -OL https://github.com/ccooper1982/wali/releases/download/$VERSION/wali-bin_$VERSION.tar.gz

# Extract
tar -xf wali-bin_$VERSION.tar.gz -C /usr/local/bin

# Install
mount -o remount,size=600M /run/archiso/cowspace

pacman -Sy --noconfirm archlinux-keyring
pacman -Q wt 2> /dev/null

if [ $? -ne 0 ]
  then
    pacman -Sy --noconfirm wt
fi

echo "--------------"
echo Change directory to $INSTALL_DIR
echo Then ./$START_SH
echo "--------------"
