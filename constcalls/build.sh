#!/usr/bin/env bash

BASE=$(cd $(dirname $0) && pwd -P)

pushd $BASE >/dev/null
source ../.localenv
clang++-11 $($LLVMCONFIG --cppflags --ldflags) -std=c++14 -g eval.cc main.cc $($LLVMCONFIG --system-libs --libs all)
RC=$?
popd >/dev/null
exit $RC
