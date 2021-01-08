#!/usr/bin/env bash

# Work from a known location
BASE=$(realpath $(dirname $0))
pushd $BASE

# Check we have a fresh copy of the test case
[[ -d linux.git ]] || git clone ssh://git@github.com/torvalds/linux linux.git
cd linux.git
git reset
git clean -dxf
make CC=clang-11 defconfig
make CC=clang-11 clean

# Switch off module signing that requires a key
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

# Extract the build-steps and reformat for analysis in clang. Strip out target binaries and
# assembly generation steps.
make --trace CC=clang-11 | tee ../make.trace
cd ..
grep 'echo.*CC.*clang-11' make.trace | grep -o '; clang-11[^;]*' >make.units
cat make.units | grep -o -- '-o [^ ]*[.]o' \
               | sed 's!^-o \(.*\).o!units/\1.ll!' >units.list
cat make.units | sed -e 's/^; clang-11 \(.*\) -o \([^ ]*\)[.]o \(.*\)$/clang-11 \1 -g -S -emit-llvm -o ..\/units\/\2.ll \3/' \
               | grep -v '^;' >build.sh
chmod +x build.sh

# Build the unit tree so that it matches the structure of source files
while read -r line
do
    dirname $line
done <units.list | sort -u | while read -r line
do
    mkdir -p $line
done

popd

