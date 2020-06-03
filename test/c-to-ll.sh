#!/bin/bash

if [ "$#" -ne 2 ]; then
  echo "c-to-ll.sh <.c file> <clang dir>"
  echo "ex)  ./c-to-ll.sh ./bubble_sort/src/bubble_sort.c llvm-10.0-releaseassert/bin"
  exit 1
fi

IRGEN=`dirname "$BASH_SOURCE"`/irgen
if [[ ! -f "$IRGEN" ]]; then
  echo "irgen is not built! please run ./build.sh <clang dir>"
  exit 1
fi

if [[ "$OSTYPE" == "darwin"* ]]; then
  ISYSROOT="-isysroot `xcrun --show-sdk-path`"
else
  ISYSROOT=
fi

CXX=$2/clang
set -e
$CXX $ISYSROOT -O1 -fno-strict-aliasing -fno-discard-value-names -g0 $1 \
    -mllvm -disable-llvm-optzns -S -o /tmp/a.ll -emit-llvm
$IRGEN /tmp/a.ll "${1%.c}.ll"
