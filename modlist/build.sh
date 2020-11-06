#!/usr/bin/env bash

source ../.localenv
clang++ $($LLVMCONFIG --cppflags --ldflags) -std=c++14 main.cc $($LLVMCONFIG --system-libs --libs all)
