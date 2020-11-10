#!/usr/bin/env bash

BASE=$(dirname $0)
pushd $BASE >/dev/null
../../funclist/a.out $(cat units.list)
popd >/dev/null
