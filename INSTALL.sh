#!/usr/bin/env bash

if [ $(uname) == "Linux" ]
then
    cat >llvm_sources <<EOF
deb http://apt.llvm.org/buster/ llvm-toolchain-buster main
deb-src http://apt.llvm.org/buster/ llvm-toolchain-buster main
# 10
deb http://apt.llvm.org/buster/ llvm-toolchain-buster-10 main
deb-src http://apt.llvm.org/buster/ llvm-toolchain-buster-10 main
# 11
deb http://apt.llvm.org/buster/ llvm-toolchain-buster-11 main
deb-src http://apt.llvm.org/buster/ llvm-toolchain-buster-11 main
EOF
    echo "wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -"
    echo "sudo mv llvm_sources /etc/apt/sources.list.d/llvm.list"
    echo "sudo apt update"
    echo "sudo apt install llvm-11 llvm-11-dev llvm-11-tools flex bison clang-11"
    echo >.localenv "export LLVMCONFIG="$(dpkg -L llvm-11 | grep llvm-config-11)
    echo >>.localenv "export CC=clang-11"
    echo >>.localenv "export CPPC=clang++-11"
elif [ $(uname) == "Darwin" ]
then
    echo "brew install llvm"
    echo >.localenv "export LLVMCONFIG="$(brew list llvm | grep bin.*llvm-config | head -n1)
    echo >>.localenv "export CC="$(brew list llvm | grep bin.*clang$ | head -n1)
    echo >>.localenv "export CPPC="$(brew list llvm | grep bin.*clang++$ | head -n1)
fi
