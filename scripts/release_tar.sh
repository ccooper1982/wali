#!/bin/sh

VERSION=v0.5

meson configure --buildtype=release build

if [ $? -ne 0 ]; then
    exit 1
fi

meson compile -C build

if [ $? -ne 0 ]; then
    exit 1
else
    if [ -d release ]; then
        rm -rd release
    fi

    mkdir -p release/wali/wwwroot

    cp wali.css wt_config.xml collapse.gif release/wali/wwwroot
    cp build/wali scripts/start.sh scripts/install.sh release/wali

    cd release
    tar -czf wali-bin_$VERSION.tar.gz wali
    cd -
fi
