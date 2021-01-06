#!/usr/bin/env bash

BASE=$(realpath $(dirname $0))
pushd $BASE

[[ -d linux.git ]] || git clone ssh://git@github.com/torvalds/linux linux.git
cd linux.git

make clean
make --trace >>../make.trace

popd

