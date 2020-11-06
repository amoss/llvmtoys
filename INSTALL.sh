#!/usr/bin/env bash

if [ $(uname) == "Linux" ] 
then
    echo "apt install llvm-11 llvm-11-dev llvm-11-tools"
    echo >.localenv "export LLVMCONFIG="$(dpkg -L llvm-11 | grep llvm-config-11)
elif [ $(uname) == "Darwin" ] 
then
    echo "brew install llvm"
    echo >.localenv "export LLVMCONFIG="$(brew list llvm | grep bin.*llvm-config | head -n1)
fi
