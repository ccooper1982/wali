#!/bin/sh

mount -o remount,size=600M /run/archiso/cowspace

pacman -Sy --noconfirm archlinux-keyring
pacman -Q wt 2> /dev/null

if [ $? -ne 0 ]
  then
    pacman -S --noconfirm wt
fi
