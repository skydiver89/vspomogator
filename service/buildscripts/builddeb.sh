#!/bin/bash
ver=`git describe --tags --abbrev=0`
fpm \
    -s dir -t deb \
    -p ./build/vspomogator-$ver-x86_64.deb \
    -f \
    -n vspomogator \
    --license MIT \
    -v $ver \
    -a x86_64 \
    -m "skydiver89 <maslov140@gmail.com>" \
    --description "ESP32 macro-keyboard" \
    --url "https://github.com/skydiver89/vspomogator" \
    --after-install ./buildscripts/postinst.sh \
    --before-remove ./buildscripts/preremove.sh \
    ./build/vspomogator=/usr/local/bin/vspomogator \
    vspomogator.yml=/etc/vspomogator/vspomogator.yml \
    ./buildscripts/vspomogator.service=/etc/systemd/system/vspomogator.service
exit 0
