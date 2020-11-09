#!/usr/bin/env bash

BASE=$(realpath $(dirname $0))
pushd $BASE
[[ -d netdata.git ]] || git clone ssh://git@github.com/netdata/netdata netdata.git
cd netdata.git

git checkout master
./netdata-installer.sh --install $BASE/install --dont-wait --disable-telemetry

make clean
make --trace >>../make.trace
cd ..
grep ^gcc make.trace >make.units

python3 common.py >build.sh

popd

