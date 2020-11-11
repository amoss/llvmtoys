#!/usr/bin/env bash

BASE=$(dirname $0)
pushd $BASE >/dev/null
../../callgrep/a.out $(cat units.list)
popd >/dev/null
