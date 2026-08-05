#!/bin/bash
dt=`git show --no-patch --format=%ci`
dt=${dt//-/.}
dt=${dt//:/.}
dt=${dt// /-}
dt=`echo $dt | cut -c 1-19`
fpm \
    -s dir -t deb \
    -p vspomogator-$dt-x86_64.deb \
    -f \
    -n bspomogator \
    --license custom \
    -v $dt \
    -a x86_64 \
    -m "Skydiver89 <maslov140@gmail.com>" \
    --vendor "Lulz inc." \
    --description "ESP32 macro-keyboard" \
    --url "http://github.com/skydiver89/vspomogator" \
    vspomogator=/usr/local/bin/vspomogator \
    vspomogator.yml=/etc/vspomogator/vspomogator.yml
exit 0
