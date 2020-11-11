#!/usr/bin/env bash

BASE=$(realpath $(dirname $0))

pushd $BASE >/dev/null
source ../.localenv
clang++ $($LLVMCONFIG --cppflags --ldflags) -std=c++14 -g main.cc $($LLVMCONFIG --system-libs --libs all)
popd >/dev/null
