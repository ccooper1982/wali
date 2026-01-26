#!/bin/sh

VERSION=v0.9

# default options should be ok, but be explicit
meson configure -Dskip_validation=false -Ddisable_install=false -Dfake_data=false --buildtype=release build

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

    cp -r wwwroot release/wali
    cp build/wali scripts/start.sh scripts/install.sh release/wali

    cd release
    tar -czf wali-bin_$VERSION.tar.gz wali
    cd -
fi
