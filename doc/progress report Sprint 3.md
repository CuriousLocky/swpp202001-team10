
This is the Progress report for Sprint 3.

# Table of Contents

- [Table of Contents](#table-of-contents)
  - [Backend Rework](#backend-rework)
    - [Development status](#development-status)
      - [`reset [heap|stack]` support](#reset-heapstack-support)
        - [`reset stack` example](#reset-stack-example)
        - [`reset heap` example](#reset-heap-example)
      - [Register allocation support](#register-allocation-support)
      - [Better `stack` management](#better-stack-management)
      - [`1`, `2`, `4` byte data type stack storage support](#1-2-4-byte-data-type-stack-storage-support)
    - [Cost change](#cost-change)
  - [`malloc` to `alloca`](#malloc-to-alloca)
    - [Development status](#development-status-1)
    - [Future Plan](#future-plan)

## Backend Rework

Implemented by Hexiang Geng and Minghang Li.

### Development status

The backend currently is passing all the tests provided. The following optimizations are provided along with the implementation of the new backend.

#### `reset [heap|stack]` support

When we access `heap` memory after accessing `stack` memory or vice versa, it will always be more efficient to first `reset [stack|heap]` to reduce the cost of head moving.

##### `reset stack` example

**Before**

```llvm
; This is part of the code in rmq2d_sparsetable
    store 8 r1 20480 0
    store 8 0 sp 104
;...other code continues
```

**After**

```llvm
; This is the same part of code in rmq2d_sparsetable
    store 8 r2 20480 0
    reset stack
    store 4 0 sp 0
;...other code continues
```

##### `reset heap` example

Due to the difference in implementation, the assembly emitted are quite different from the original assembly. Here we just want to show the effect of inserting `reset heap`.

**Before**

```llvm
; ...Some lines of code omitted
  .for.cond:
    r1 = load 4 20496 0
    store 8 r1 sp 112
    r1 = load 8 sp 96
    r2 = load 8 sp 112
    r1 = icmp slt r1 r2 32
    store 8 r1 sp 120
    r1 = load 8 sp 120
    br r1 .for.body .for.cond.cleanup
;...other code continues
```

**After**

```llvm
; ...Some lines of code omitted
  .for.cond:
    r1 = load 4 sp 0
    reset heap
    r2 = load 4 20496 0
    r16 = icmp slt r1 r2 32
    br r16 .for.body .for.cond.cleanup
;...other code continues
```

#### Register allocation support

The `SimpleBackend` depromote all registers to `alloca`s, which will bring huge cost since there are quite a few `store`s and `load`s which will have at least a cost of `2`.

In `LessSimpleBackend`, the 16 registers provided can be utilized very well. We can observe clearly that the multiple `load`s and `store`s are eliminated.

**Before**

```llvm
start countSetBits 1:
  .entry:
    ; init sp!
    sp = sub sp 64 64
    store 8 arg1 sp 8
    store 8 0 sp 24
    r1 = load 8 sp 8
    store 8 r1 sp 0
    r1 = load 8 sp 24
    store 8 r1 sp 16
    br .while.cond
;...some lines of code omitted here
  .while.body:
    r1 = load 8 sp 0
    r1 = and r1 1 32
    store 8 r1 sp 40
    r1 = load 8 sp 16
    r2 = load 8 sp 40
    r1 = add r1 r2 32
    store 8 r1 sp 48
    r1 = load 8 sp 0
    r1 = lshr r1 1 32
    store 8 r1 sp 56
    r1 = load 8 sp 56
    store 8 r1 sp 8
    r1 = load 8 sp 48
    store 8 r1 sp 24
    r1 = load 8 sp 8
    store 8 r1 sp 0
    r1 = load 8 sp 24
    store 8 r1 sp 16
    br .while.cond
;...other code continues
```
**After**

```llvm
start countSetBits 1:
  .entry:
    sp = sub sp 8 64
    store 4 arg1 sp 0
    store 4 0 sp 4
    br .while.cond
;...some lines of code omitted here
 .while.body:
    r3 = and r1 1 32
    r4 = add r2 r3 32
    r2 = lshr r1 1 32
    store 4 r2 sp 0
    store 4 r4 sp 4
    br .while.cond
;...other code continues
```

#### Better `stack` management

In `LessSimpleBackend`, variables stored on `stack` can be automatically poped based on the position it is used. This is a more agile way of managing `stack` memory, since it enables the stack memory slot to be used by multiple objects.

We also support lazy `alloca`, meaning delaying the memory allocation as much as possible so that we can better utilize the stack space, making the head movement as small as possible.

#### `1`, `2`, `4` byte data type stack storage support

This is yet another method to minimize the head movement by allowing to store variables with `i8`, `i16`, `i32` data type rather than all `i64`. The alignment of each variables on stack are finely tuned.

This effect of this optimization is trivially shown in the ASM provided above. Thus no specific before & after comparison is provided here.

### Cost change

The new backend (`LessSimpleBackend` + `NewAssemblyEmitter`) can provide hugh (more than `50%`) cost reduce on every test case compared with `SimpleBackend` + `AssemblyEmitter`, *without* any passes enabled.

```
==gcd==
Average cost improve %: 54.450%
======
==matmul1==
Average cost improve %: 64.422%
======
==matmul3==
Average cost improve %: 56.687%
======
==rmq1d_naive==
Average cost improve %: 53.721%
======
==rmq2d_sparsetable==
Average cost improve %: 61.367%
======
==bitcount3==
Average cost improve %: 63.697%
======
==bitcount5==
Average cost improve %: 71.400%
======
==binary_tree==
Average cost improve %: 58.899%
======
==prime==
Average cost improve %: 57.984%
======
==collatz==
Average cost improve %: 59.079%
======
==matmul4==
Average cost improve %: 71.678%
======
==bubble_sort==
Average cost improve %: 64.089%
======
==matmul2==
Average cost improve %: 68.589%
======
==rmq1d_sparsetable==
Average cost improve %: 63.667%
======
==bitcount2==
Average cost improve %: 61.353%
======
==bitcount4==
Average cost improve %: 69.099%
======
==rmq2d_naive==
Average cost improve %: 60.687%
======
==bitcount1==
Average cost improve %: 64.286%
======
```

## `malloc` to `alloca`

Implemented by Alfiya Mussabekova.

### Development status

The current version of optimization is causing `Segmentation Fault` in `test/prime`. It's still not working and requires further debugging.

### Future Plan

All team members will try to help fix the bug. If it could not be fixed, then we would not adapt this implementation.
