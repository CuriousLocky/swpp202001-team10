
This is the Progress report for Sprint 2

# Table of Contents

- [Table of Contents](#table-of-contents)
  - [Backend rework](#backend-rework)
    - [Development status](#development-status)
      - [Sample optimization result](#sample-optimization-result)
    - [Future plan](#future-plan)
  - [`malloc` to `alloca`](#malloc-to-alloca)
    - [Development state](#development-state)
    - [Future plan](#future-plan-1)
  - [Import Existing Passes](#import-existing-passes)
    - [Dead Code Elimination](#dead-code-elimination)
      - [Before:](#before)
      - [After:](#after)
    - [Lowering `alloca` to Registers](#lowering-alloca-to-registers)
      - [Before:](#before-1)
      - [After:](#after-1)
    - [`global` Optimization](#global-optimization)
      - [Before](#before-2)
      - [After](#after-2)
    - [Test result](#test-result)
  - [Abandoned Work](#abandoned-work)
  - [Misc](#misc)

## Backend rework

Implemented by Hexiang Geng and Minghang Li. It's unfinished, so no test IRs/assembly code are provided.

### Development status

Implement IR-level register allocation and stack management in this Sprint. IR-level stack pointer offset is also supproted in the new backend. The garbage collection and reuse of registers is also possible. Variables will be allocated to registers as many as possible. When there are no registers to use, the variables can be stored 

#### Sample optimization result

**Before**

```llvm
define void @main(i32 %x, i32 %y) {
; CHECK: start main 0:
  %temp1 = call i32 @func(i32 1, i32 2, i32 3)
  %temp2 = call i32 @func(i32 %temp1, i32 2, i32 3)
  %temp3 = call i32 @func(i32 %temp1, i32 %temp2, i32 3)
  %temp4 = call i32 @func(i32 %temp1, i32 %temp2, i32 %temp3)
  %temp5 = call i32 @func(i32 %temp2, i32 %temp3, i32 %temp4)
  %temp6 = call i32 @func(i32 %temp1, i32 %temp2, i32 %temp5)
  ret void
}
; CHECK: end main
declare i32 @func(i32, i32, i32)
```

**After**

```llvm
; To show the success of stack management, only 4 registers are allowed 
define void @main(i32 %arg0, i32 %arg1) {
entry:
  ; While translating into assembly, only the name of the register will be
  ; preserved. For example, r1_temp1 will just be translated into r1
  %r1_temp1 = call i32 @func(i32 1, i32 2, i32 3)
  %r2_temp2 = call i32 @func(i32 %r1_temp1, i32 2, i32 3)
  %r3_temp3 = call i32 @func(i32 %r1_temp1, i32 %r2_temp2, i32 3)
  %r4_temp4 = call i32 @func(i32 %r1_temp1, i32 %r2_temp2, i32 %r3_temp3)
  ; ----------------------------------------------------------------------------
  ; The following 3 lines will be squashed into 1 line in assembly, 
  ; which is a simple 'store 4 r1 sp 0' 
  %temp_p_r1_temp1 = call i8* @__spOffset(i64 0)
  %temp_p_r1_temp1_cast = bitcast i8* %temp_p_r1_temp1 to i32*
  store i32 %r1_temp1, i32* %temp_p_r1_temp1_cast
  ; ----------------------------------------------------------------------------
  %r1_temp5 = call i32 @func(i32 %r2_temp2, i32 %r3_temp3, i32 %r4_temp4)
  %temp_p_r1_temp5 = call i8* @__spOffset(i64 4)
  %temp_p_r1_temp5_cast = bitcast i8* %temp_p_r1_temp5 to i32*
  store i32 %r1_temp5, i32* %temp_p_r1_temp5_cast
  %temp_p_r1_temp11 = call i8* @__spOffset(i64 0)
  %temp_p_r1_temp11_cast = bitcast i8* %temp_p_r1_temp11 to i32*
  %r1_r1_temp1 = load i32, i32* %temp_p_r1_temp11_cast
  %temp_p_r1_temp52 = call i8* @__spOffset(i64 4)
  %temp_p_r1_temp52_cast = bitcast i8* %temp_p_r1_temp52 to i32*
  %r3_r1_temp5 = load i32, i32* %temp_p_r1_temp52_cast
  %r4_temp6 = call i32 @func(i32 %r1_r1_temp1, i32 %r2_temp2, i32 %r3_r1_temp5)
  ret void
}

declare i32 @func(i32, i32, i32) 

declare void @__resetHeap(void) ; Dummy function for resetting heap

declare void @__resetStack(void) ; Dummy function for resetting stack

declare i8* @__spOffset(i64) ; Dummy function for offsetting stack pointer
```

### Future plan

There are quite a few features that we have not yet figured out a concrete way to implement. The followings are some of the features that we finished designing but not implemented yet:

- Assembly emission: registers assignments and some dummy-functions for operating memory read head are created at IR-level. Those instructions need to be translated (emitted?) properly into assembly.
- Supports for `phi` operation: all `phi` operations would be put on stack to prevent using before initialization or register collision.
- Type conversion for values of different lengths: Different from SimpleBackend, we plan to use more data type other than `i64` and `i8*`. Plan to implement by bit operation. 

## `malloc` to `alloca`

Implemented by Alfiya Mussabekova

### Development state

From last sprint there left unsolved assertion fail `Assertion New->getType() == getType() && replaceAllUses of value with new value with different type`, to fix it, we decided to allocate array of `i8`, because return type of malloc is `i8*`.

However, while implementing we changed array of `i8` to many `i8` and added checking on how much bytes are allocated, for now we decided that it is safe to allocate 256 bytes.

Optimization is completed and works in my teammate's computer, but we did not have enough time to test it. Most probably in my computer there is environment problem, because of which I get another assertion fail: `isValidArgumentType(Params[i]) && "Not a valid type for function argument!"`

### Future plan

- Fix environment problem
- Complete testing
- Fix potential bugs


## Import Existing Passes

Implemented by Jasmine Abtahi. In this sprint, the following three existing IR passes were integrated in the project this week. 

### Dead Code Elimination

DCE pass is a function level pass that checks if an instruction is unused and removes them. 

#### Before:

```llvm
define i32 @main(i32 %x, i32 %y){
    %z = add i32 %x, %y
    ret i32 %x
}
```

#### After:

```llvm
define i32 @main(i32 %x, i32 %y){
    ret i32 %x
}
```

### Lowering `alloca` to Registers

TA gives IR program that already has allocas lowered into assembly if possible. However, it will still have many loads/stores which can be optimized. `mem2reg.h` didn't affect the way expected so we had to write a pass called `alloca2reg` that uses the function `PromoteMemToReg()` from `promotemem2reg.h`. The new program optimizes all the unnecessary loads and stores.

#### Before:

```llvm
define i32 @main(i32 %x) {
    %p = alloca i32
    br label %end
end:
    store i32 %x, i32* %p
    %q = load i32, i32* %p
    ret i32 %q
}

;generated assembly
start main 1:
  .entry:
    ; init sp!
    sp = sub sp 24 64
    r1 = add sp 16 64
    store 8 r1 sp 0
    br .end
  .end:
    r2 = load 8 sp 0
    store 4 arg1 r2 0
    r1 = load 8 sp 0
    r1 = load 4 r1 0
    store 8 r1 sp 8
    r1 = load 8 sp 8
    ret r1
end main
```

#### After:

```assembly
start main 1:
  .entry:
    br .end
  .end:
    ret arg1
end main
```

### `global` Optimization 

It's a module level pass that deletes unused global variables that never have their address taken.

#### Before

```llvm
@variable = internal constant i32 45 
define i32* @main(i32* %x) {
    ret i32* %x
}
; ModuleID = 'DepromotedModule'
source_filename = "DepromotedModule"
declare void @resetHeap(void)
declare void @resetStack(void)
define i32* @main(i32* %__arg1__) {
.entry:
  %__r1__ = call i8* @malloc(i64 8)
  ret i32* %__arg1__
}
declare i8* @malloc(i64)
```
#### After

```llvm
; ModuleID = 'DepromotedModule'
source_filename = "DepromotedModule"
declare void @resetHeap(void)
declare void @resetStack(void)
define i32* @main(i32* %__arg1__) {
.entry:
  ret i32* %__arg1__
}
declare i8* @malloc(i64)
```

### Test result

All these three implementations passed TA's test files and our filechecks succussfully. Regarding the cost improvements, unfortunately we were not able to get TA's tests improved due to the added complexity and the passes order. After revising the order, we will see the real improvements. 

```
==bitcount3==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==gcd==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==binary_tree==

Average cost improve %: -0.01932%  
Average heapUsage improve %: 0.00000%

==bitcount4==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==bitcount2==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==bitcount1==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==collatz==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==prime==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==bubble_sort==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%

==bitcount5==

Average cost improve %: 0.00000%  
Average heapUsage improve %: 0.00000%
```

## Abandoned Work

The two optimizations, re-ordering memory access and using registers as cache, are abandoned, due to the high difficulty in implementation and rather low improvement. The abolition is agreed by all the team members. The team members working on these two projects were involved in re-writing the backend.

## Misc

Two automation scripts were provided by Minghang Li to help on testing the improvements on the code.