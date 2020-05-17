
This is the requirement and specification document for Team 10 that describes the optimization choices and implementation overviews.

# Table of Contents

* [Sprint 1 Optimization](#sprint-1-optimization)
    * [Arithematic optimizations](#arithematic-optimizations)
    * [Reset insertion](#reset-insertion)
    * [`malloc` to `alloca`](#malloc-to-alloca)
    * [Existing LLVM optimization](#existing-llvm-optimization)
* [Sprint 2 Optimization](#sprint-2-optimization)
* [Sprint 3 Optimization](#sprint-3-optimization)

## Sprint 1 Optimization

### Arithematic optimizations

#### Description

The SWPP machine has a very weird ISA, which makes some (usually not-so-costly) arithmetic instructions costier than others. By replacing the rather costly instructions with cheaper ones we can save the cost up to 50%. The instructions to be replaced are listed as follows.

| Instruction | Replacable Instruction | Saved Cost |
|---|:---:|---:|
| `ashr` | `udiv` |    25% |
| `shl`  | `mul`  |    25% |
| `add`  | `mul`  |    50% |
| `sub`  | `mul`  |    50% |

#### Algorithmic Implementation

```python
for each use of Instruction i in input.ll
    if Instruction i matches with Pattern p at index j
        replace the use with replacable Instruction r at index j
    end
end
```

#### Example IR

**Case 1**: `shift` to `mul`/`div`
``` llvm
%1 = shl  %a, 1, 32 ;before
%1 = mul  %a, 2, 32 ;after
%2 = ashr %b, 1, 32 ;before
%2 = udiv %b, 2, 32 ;after
```

**Case 2**: `add`/`sub` val with val's multiplicands to `mul`

``` llvm
%1 = add  %a, %a, 32 ;before
%1 = mul  %a, 2,  32 ;after
```
**Case 3**: `add`/`sub` with zero to `mul`

``` llvm
%1 = add  %a, 0,  32 ;before
%1 = mul  %a, 1,  32 ;after
%2 = sub  %b, 0,  32 ;before
%2 = mul  %b, 1,  32 ;after
%3 = sub   0, %c, 32 ;before
%3 = mul  -1, %c, 32 ;after
```

### Reset insertion

#### Description

The SWPP machine has a tape-like memory with no random access support. Therefore, the cost of moving along the tape is considerable, especially whilst accessing heap memory and stack memory interleavingly.

However, there's a `reset [stack|heap]` instruction provided, with a fixed cost of `2`, which is equivalent to moving the tap-access head a disteance of `5000`. Since the distance between `10240` (stack starting address) and `20480` (heap starting address) is obviously larger than `5000`, it would always be more efficient to `reset` when you want to access heap after stack (or vice versa).

#### Algorithmic Implementation

```c++
void reset_insert(Function F):
    for(BasicBlock B in F.blocks()):
        for(Instruction I in B.instructions()):
            if(!I.accessMemory()):
                continue
            bool prev_access_found_flag = false
            //checking instructions in the same block
            for(Instruction I_prev in B.instructionsBefore(I)[:-1]):
                if(!I.accessMemory()):
                    continue
                prev_access_found_flag = true
                Instruction res_ins = determin_res(I, I_prev)
                if(res_ins != NULL):
                    B.insertBefore(res_ins, I)
            if(prev_access_found_flag):
                continue
            // if B has multiple predecessors, not travil to determine
            // may implement in the future
            if(B.predBlocks().size() != 1):
                continue
            BasicBlock B_prev = B.predecessors()[0]
            while(!prev_access_found_flag):
                for(Instruction I_prev in B_prev.instructions()[:-1]):
                    if(!I_prev.accessMemory()):
                        continue
                    prev_access_found_flag = true
                    Instruction res_ins = determin_res(I, I_prev)
                    if(res_ins != NULL):
                        B.insertBefore(res_ins, I)
                    break
                if(prev_access_found_flag)
                    break
                if(B_prev.predecessors().size()!=1):
                    break
                B_prev = B_prev.predecessors()[0]

Instruction determin_res(Instruction later, Instruction former):
    MemAccPos pos_l = later.getMemAccPos()
    MemAccPos pos_f = later.getMemAccPos()
    if(pos_l == MemAccPos.STACK && pos_f == MemAccPos.HEAP):
        return Instruction.RESET_STACK
    if(pos_l == MemAccPos.HEAP && pos_f == MemAccPos.STACK):
        return Instruction.RESET_HEAP
    return NULL
```

#### Example IR

**Case 1**：Stack visited, heap to-visit

Before

```llvm
define i32 @main(i32 %x){
    %a = call i32* @malloc(i32 8)
    store i32 3, i32* %ptr

    ... ;some instructions that dont access memory

    %b = alloca i32, i32 4

    ... ;some other code
}
```
After

```llvm
define i32 @main(i32 %x){
    %a = call i32* @malloc(i32 8)
    store i32 3, i32* %ptr

    ... ;some instructions that dont access memory

    reset stack
    %b = alloca i32, i32 4

    ... ;some other code
}
```

**Case 2**: Heap visited, stack to-visit

Before

```llvm
define i32 @main(i32 %x){
    %b = alloca i32, i32 4

    ... ;some instructions that dont access memory

    %a = call i32* @malloc(i32 8)
    store i32 3, i32* %ptr

    ... ;some other code
}
```
After

```llvm
define i32 @main(i32 %x){
    %b = alloca i32, i32 4

    ... ;some instructions that dont access memory

    %a = call i32* @malloc(i32 8)
    reset heap
    store i32 3, i32* %ptr

    ... ;some other code
}
```
**Case 3**: Unknown visited / to-visit (Optimization not applied)

Before

```llvm
define i32 @main(i32* %x){
    %b = load i32, i32* %x

    ... ;some instructions that dont access memory

    %a = call i32* @malloc(i32 8)
    store i32 3, i32* %ptr

    ... ;some other code
}
```

After

```llvm
define i32 @main(i32* %x){
    %b = load i32, i32* %x

    ... ;some instructions that dont access memory

    %a = call i32* @malloc(i32 8)
    store i32 3, i32* %ptr

    ... ;some other code
}
```

### `malloc` to `alloca`

#### Description

Observe that `malloc` is costlier than `alloca`. If the memory chunk allocated by `malloc` is freed before returning, in essence it is equivalent to allocating on the stack. Therefore, we can convert `malloc` to `alloca` under this circumstance. Note that the stack space is significantly small, so a threshold of allocated size should be set to prevent stack overflow.

#### Algorithmic Implementation

```c++
// Find all malloc variables
vector <Instruction> mall;
for (BasicBlock BB : F)
    for (Instruction I : BB) {
        if I matches malloc
            add to mall
    }
// Check whether the instruction is freed before end of function, if not, remove
bool flag = true
foreach m in mall {
    for (BasicBlock BB : F)
        for (Instruction I : BB) {
            if I matches free and m in arguments
                flag = false
            }
    if (flag)
        remove m from mall
}
// for each instruction in mall change malloc to alloca
foreach m in mall {
    change malloc to alloca
}
```

#### Example IR

**Case 1**: Optimiztion applied

Before

```llvm
define i32 @f(i32 %x, i32 %y) {
    %malloc_var = call i8* @malloc(i32 4)
    ...
    call void @free(i8* %malloc_var)
    ret 0
}
```
After

```llvm
define i32 @f(i32 %x, i32 %y) {
    %alloca_var = alloca %i32
    ...
    ret 0
}
```

**Case 2**: `malloc` is not freed, no replace

Before

```llvm
define i32 @f(i32 %x, i32 %y) {
    %malloc_var = call i8* @malloc(i32 4)
    ...
    ret 0
}
```
After
```llvm
define i32 @f(i32 %x, i32 %y) {
    %malloc_var = call i8* @malloc(i32 4)
    ...
    ret 0
}
```

**Case 3**: `malloc` size is larger than 10240 (maximum stack size) -> no replace

Before

```llvm
define i32 @f(i32 %x, i32 %y) {
    %malloc_var = call i8* @malloc(i32 10244)
    ...
    ret 0
}
```

After

```llvm
define i32 @f(i32 %x, i32 %y) {
    %malloc_var = call i8* @malloc(i32 10244)
    ...
    ret 0
}
```

### Existing LLVM optimization

#### Description

According to `optList.md`, there are some existing optimization options provided by the LLVM framework such as: dead argument elimination, function inlining, tail call elimination and constant folding. We would like to use them in our compiler.

This part would be mainly utilizing existing passes and libraries. The difficulty, procedure and output of the integration cannot be evaluated for now. What's more, there are multiple optimization options to integrate. Hence, no pseudocode or sample IRs will be provided.

Existing optimization to be integrated in Sprint 1:

* Function inlining
* Dead argument elimination
* Constant folding


## Sprint 2 Optimization

### Existing LLVM optimization
#### Description

We are going to implement the follwoing existing optimizations:

* Branch-related optimizations including br -> switch
* Loop Interchange
* Loop Invariant Code Motion

=======
### Re-ordering memory access

#### Description

The target machine is not capable to perform random access, so if the code requires to frequently jump between stack and heap, it will cost a lot. The goal of this optimization is to re-order the instuctions with memory access to minimize the cost of moving the memroy access head.

#### Algorithmic Implementation

The algorithm to implement this opt is still unclear at the time of writing. 

#### Example IR

**Case 1**:No dependency between accesses and re-ordering benifits

Before

```llvm
define i32 @f(i32 %x, i32 %y) {
    %malloc_var_1 = call i8* @malloc(i32 8)
    store i32 3, i32* %malloc_var_1
    %alloca_var = alloca i32, i32 4
    %malloc_var_2 = call i8* @malloc(i32 8)
    store i32 4, i32* %malloc_var_2
    ...
    ret 0
}
```

After

```llvm
define i32 @f(i32 %x, i32 %y) {
    %alloca_var = alloca i32, i32 4
    %malloc_var_1 = call i8* @malloc(i32 8)
    store i8 3, i8* %malloc_var_1
    %malloc_var_2 = call i8* @malloc(i32 8)
    store i8 4, i8* %malloc_var_2
    ...
    ret 0
}
```

**Case 2**:There is dependency between the instructions(opt not applied)

Before

```llvm
define i32 @f(i32 %x, i32 %y) {
    %alloca_ptr_1 = alloca i32, i32 8
    ...
    %alloca_content_1 = load i32, i32* %alloca_ptr
    %malloc_var = call i8* @malloc(%alloca_content)
    store i8 4, i8* %malloc_var
    %alloca_ptr_1 = alloca i8*, %malloc_var
    ...
}
```

After

```llvm
define i32 @f(i32 %x, i32 %y) {
    %alloca_ptr_1 = alloca i32, i32 8
    ...
    %alloca_content_1 = load i32, i32* %alloca_ptr
    %malloc_var = call i8* @malloc(%alloca_content)
    store i8 4, i8* %malloc_var
    %alloca_ptr_1 = alloca i8*, %malloc_var
    ...
}
```

**Case 3**:Re-ordering does not help(not applied)

Before

```llvm
define i32 @f(i32 %x, i32 %y) {
    %alloca_var = alloca i32, i32 4
    %malloc_var_1 = call i8* @malloc(i32 8)
    store i8 3, i8* %malloc_var_1
    %malloc_var_2 = call i8* @malloc(i32 8)
    store i8 4, i8* %malloc_var_2
    ...
    ret 0
}
```

After

```llvm
define i32 @f(i32 %x, i32 %y) {
    %alloca_var = alloca i32, i32 4
    %malloc_var_1 = call i8* @malloc(i32 8)
    store i8 3, i8* %malloc_var_1
    %malloc_var_2 = call i8* @malloc(i32 8)
    store i8 4, i8* %malloc_var_2
    ...
    ret 0
}
```

### Existing LLVM optimization integration

This part would be mainly utilizing existing passes and libraries. The difficulty, procedure and output of the integration cannot be evaluated for now. What's more, there are multiple optimization options to integrate. Hence, no pseudocode or sample IRs will be provided.

Existing optimization to be integrated in Sprint 2:

* Redundant code elimination
* Lowering alloca to register
* Global var to local

## Sprint 3 Optimization

To be updated.