#!/usr/bin/env bash

BASE=$(realpath $(dirname $0))
pushd $BASE

clang flowgraph.c -emit-llvm -S -o flowgraph-raw.ll
sed -e's/^\(attributes.* \)optnone \(.*\)$/\1\2/' flowgraph-raw.ll >flowgraph.ll

popd


