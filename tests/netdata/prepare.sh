#!/usr/bin/env bash

SOURCES="daemon/main.c daemon/daemon.c daemon/signals.c"
CONFIG='-DTARGET_OS=1 -DVARLIB_DIR="'$1'/install/var/lib/netdata" -DCACHE_DIR="'$1'/install/var/cache/netdata" -DCONFIG_DIR="'$1'/install/etc/netdata" -DLIBCONFIG_DIR="'$1'/install/usr/lib/netdata/conf.d" -DLOG_DIR="'$1'/install/var/log/netdata" -DPLUGINS_DIR="/install/usr/libexec/netdata/plugins.d" -DRUN_DIR="$1/install/var/run/netdata" -DWEB_DIR="'$1'/install/share/web"'

mkdir -p daemon
for src in $SOURCES
do
    clang-11 -g -S -emit-llvm $CONFIG -I$1 -I$1/externaldeps/jsonc $1/$src -o $src.ll
done
