#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "build-irgen.sh <clang dir>"
  echo "ex)  ./build-irgen.sh ~/llvm-10.0-releaseassert/bin"
  exit 1
fi

if [[ "$OSTYPE" == "darwin"* ]]; then
  ISYSROOT="-isysroot `xcrun --show-sdk-path`"
else
  ISYSROOT=
fi

echo "----- build -----"
LLVMCONFIG=$1/llvm-config
CXXFLAGS=`$LLVMCONFIG --cxxflags`
LDFLAGS=`$LLVMCONFIG --ldflags`
LDFLAGS="$LDFLAGS -Wl,-rpath,`$LLVMCONFIG --libdir`"
LIBS=`$LLVMCONFIG --libs core irreader bitreader passes support analysis asmparser --system-libs`
SRCROOT=`$LLVMCONFIG --src-root`

CXX=$1/clang++
CXXFLAGS="$CXXFLAGS -std=c++17 -I${SRCROOT}/include"
set -e

$CXX $ISYSROOT $CXXFLAGS $LDFLAGS $LIBS irgen.cpp -lpthread -lm \
     -o irgen
