# swpp202001-tests

This repository contains C programs as well as input/output datasets for testing
[SWPP compiler](https://github.com/snu-sf-class/swpp202001-compiler)
and [interpreter](https://github.com/snu-sf-class/swpp202001-interpreter).

### How to build .ll file from your own C program?

Build `irgen` (unoptimized .ll -> mem2reg-ed .ll converter) first

```
./build-irgen.sh <clang dir>
# e.g. ./build-irgen.sh ~/llvm-10.0-releaseassert/bin
```

Then, use c-to-ll.sh to convert .c to mem2reg-ed .ll
  
```
./c-to-ll.sh <.c file> <clang dir>"
# e.g.  ./c-to-ll.sh ./bubble_sort/src/bubble_sort.c llvm-10.0-releaseassert/bin
```

### Authors

Sung-Hwan Lee: binary_tree, bubble_sort, collatz, gcd, prime

Juneyoung Lee: bitcountN, scripts
