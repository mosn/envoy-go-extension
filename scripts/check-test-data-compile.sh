#!/bin/bash

set -o errexit

cd test/http/golang/test_data

mods=`ls`

for mod in $mods; do
  cd $mod
  echo "compiling go to shared so in $mod"
  go build \
	  -v \
	  --buildmode=c-shared \
	  -o libgolang.so \
	  .
	cd ..
done