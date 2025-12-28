#!/bin/sh

mount -o remount,size=600M /run/archiso/cowspace

pacman -Sy
pacman -Q wt

if [ $? -ne 0 ]
  then
    echo "Installing wt"
    pacman -S --noconfirm wt
fi
