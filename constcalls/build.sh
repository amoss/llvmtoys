#!/usr/bin/env bash

BASE=$(cd $(dirname $0) && pwd -P)

pushd $BASE >/dev/null
source ../.localenv
clang++ $($LLVMCONFIG --cppflags --ldflags) -std=c++14 -g main.cc $($LLVMCONFIG --system-libs --libs all)
RC=$?
popd >/dev/null
exit $RC
