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

TARGETSTEP=$(grep GEN.*-o\ netdata\  make.trace | head -n1)
echo $TARGETSTEP | grep -o '[^ ]*[.]o' | sed 's!^\(.*\).o!units/\1.ll!' >units.list
echo $TARGETSTEP | grep -o '[^ ]*[.]a' >staticlibs.list
echo $TARGETSTEP | grep -o -- '-l[a-zA-Z0-9]*' >extlibs.list

popd

