#!/usr/bin/env bash

BASE=$(realpath $(dirname $0))
pushd $BASE

[[ -d linux.git ]] || git clone ssh://git@github.com/torvalds/linux linux.git
cd linux.git

git reset
git clean -dxf
make defconfig
make clean

for x in "CONFIG_SYSTEM_TRUSTED_KEYRING" ; do
  echo $x
  sed -i -e 's/^'$x'/#'$x'/' .config
  echo "$x=n" >>.config
done
for x in "CONFIG_SYSTEM_TRUSTED_KEYS" ; do
  echo $x
  sed -i -e 's/^'$x'/#'$x'/' .config
  echo $x'=""' >>.config
done
make --trace | tee ../make.trace

popd

