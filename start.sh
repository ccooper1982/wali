#!/bin/sh

pkill awi

mkdir -p build/wwwroot
cp wt_config.xml build/wwwroot

# mkdir -p build/wwwroot/css
cp wali.css build/wwwroot

# TODO copy resources to build/wwwroot

# --docroot "build/wwwroot;/resources,/css"\
./build/awi --config build/wwwroot/wt_config.xml\
            --docroot "build/wwwroot"\
            --resources-dir build/wwwroot/resources\
            --http-address 0.0.0.0\
            --http-port 8080 -t -1;
