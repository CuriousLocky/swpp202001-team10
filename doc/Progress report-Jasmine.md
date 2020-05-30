## Import existing passes

by Abtahi

### Development state

In this sprint, we implemented the following three existing IR passes.   


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
### Lowering Alloca to Registers
TA gives IR program that already has allocas lowered into assembly if possible. However, it will still have many loads/stores which can be optimized. mem2reg.h didn't affect the way expected so we had to write a pass called alloca2reg that uses the function PromoteMemToReg() from promotemem2reg.h. The new program optimizes all the unnecessary loads and stores.
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

```llvm
start main 1:
  .entry:
    br .end

  .end:
    ret arg1
end main
```


### Global Optimization 
It's a module level pass that deletes unused global variables that never have their address taken.
#### Before:

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
#### After:

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

All these three implementations passed TA's test files and our filechecks succussfully.
### Future plan

We may want to see if we can transform inner global valiables to local. 