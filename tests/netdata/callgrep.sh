#!/usr/bin/env bash

BASE=$(dirname $0)
pushd $BASE >/dev/null
../../callgrep/a.out $1 $(cat units.list)
popd >/dev/null
