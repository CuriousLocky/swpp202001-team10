## Optimizations: Ideas & Tips for Implementation

Here is the list of optimizations that are discussed at the idea presentations.
Optimizations that can be represented as composition of smaller optimizations are omitted.

#### 1. Function outlining
- This is a creative optimization that allows using more registers with the cost of function call only.
- HotColdSplitting , llvm/Transforms/IPO/HotColdSplitting.h will do the most similar thing, but I guess it is not directly usable for this.
- Try to understand how they work. e.g) what does *post-dominated* blocks mean?
- Or, you can start with a simpler example: extracting a single block.

#### 2. Reordering memory accesses
- If there is a heap access after a stack access, inserting ‘reset heap’ will be helpful.
- Reordering memory accesses will be helpful, if we know all of their locations, to minimize the head's traveling cost.
- We should (1) Minimize alternation between heap accesses and stack accesses (2) Reorder allocations so traversal becomes as linear as possible

#### 3. Register allocation
- In LLVM, register allocation is done in much lower level; so there is no library in IR that is usable in this project.
- Instead, in the project skeleton, you can see that there is ‘DepromoteRegisters’ pass; what it does is like this.

    (1) It converts all IR registers into alloca; read/write to the IR register becomes load/store to the alloca.
    The newly created allocas have suffix `_slot`.
    
    (2) After transformation, LHS of all instructions contain the name of assembly register to use:
    `__r1__2 = add i64 __r2__, __r1__` describes addition of registers r1, r2 and assigns the result to r2 again.

- The register names will be interpreted by AssemblyEmitter, and will be directly translated to “r1 = add r2 r1 64” in assembly.
- What you’ll have to do is to (1) reduce the number of the `_slot` allocas,
(2) assign the register names (`__rN__`) carefully.

#### 4. Packing registers
- There is no (IR-level) library for this in LLVM.
- You can implement this as a part of DepromoteRegisters , or an independent pass that is earlier than DepromoteRegisters.
- Try to start with a very small and simple case!

#### 5. Rewriting loop counter to use cheaper operation
- You’ll need to understand how a loop is represented in LLVM IR.
- A loop counter is called ‘induction variable’ or ‘induction PHI’ in LLVM IR, and their form is fixed (a phi with initial value and incremented value)
- You can detect it by yourself, or you can use LoopInfo::isAuxiliaryInductionVariable .

#### 6. Malloc -> alloca conversion:
There is a relevant optimization: GlobalOptPass , include/llvm/Transforms/IPO/GlobalOpt.h
- tryToOptimizeStoreOfMallocToGlobal: Moves malloc to global variable
- processInternalGlobal: Moves an internal global variable into alloca if it is only used by a function
- To make a local transformations that moves malloc to alloca, I guess you'll need to implement this by yourself..!

#### 7. Using `sp` register

- You'll need to update ~~AssemblyWriter~~AssemblyEmitter.cpp at the project skeleton to support this.

---

Here are optimizations that can be implemented by using LLVM library.
You can call Function/Loop/ModulePassManager.addPass(PassName()) at main.cpp to add the pass to your compiler.

#### 8. Tail call elimination:
https://godbolt.org/z/FiPXm3

You can either:
- Use an existing pass: TailCallElimPass, include/llvm/Transforms/Scalar/TailRecursionElimination.h ;  
- Or you can implement it by yourself (e.g. if the pass does not consider some cases). It won’t be a discount in your grade.

#### 9. Inlining:
https://godbolt.org/z/MW-JJT

You can either:
- Use an existing pass: InlinerPass, include/llvm/Transforms/IPO/Inliner.h .
- Or you can use which functions to inline carefully, by inspecting individually and calling InlineFunction() from Transforms/Utils/Cloning.h :
```
InlineResult InlineFunction(CallBase *CB, InlineFunctionInfo &IFI,
                            AAResults *CalleeAAR = nullptr,
                            bool InsertLifetime = true);
(CallBase is just a parent class of CallInst.
InlineFunctionInfo is a class that stores info about the inlining; you can simply ignore an empty object.
AAResults maintains a data about updated alias analysis results, which is not needed, so put nullptr.
Please set InsertLifetime to false. It is not needed.)
```
- Or you can implement it by yourself!

#### 10. Lowering Allocas to IR registers
https://godbolt.org/z/UUTBHm 

- We’ll give an IR program that already has allocas lowered into assembly if possible. However, it will still have many loads/stores which can be optimized.
- If you want to do this transformation for some purpose,
you can use functions from llvm/include/llvm/Transforms/Utils/PromoteMemToReg.h :
```
bool isAllocaPromotable(const AllocaInst *AI);
void PromoteMemToReg(ArrayRef<AllocaInst *> Allocas, DominatorTree &DT,
                     AssumptionCache *AC = nullptr);
```

#### 11. Loop unrolling:
https://godbolt.org/z/EjVmTc 

- LoopUnrollPass , include/llvm/Transforms/Scalar/LoopUnrollPass.h
- However, I suggest you to implement it by yourself rather than using existing pass;
Loop unroll is activated in certain cases only. Having a fine-grained tuning will be really, really a pain. It requires much background knowledge about loop analysis.

#### 12. Loop Invariant Code Motion:
- LICMPass , include/llvm/Transforms/Scalar/LICM.h
- Adding LICMPass to your pass manager can be a bit non-trivial; Please invest some time, and you’ll find how to use it. :)

#### 13. Branch-related optimizations including br -> switch
https://godbolt.org/z/fTvKGW

- SimplifyCFGPass (include/llvm/Transforms/Scalar/SimplifyCFG.h) will do the transformations.

#### 14. Constant folding, removing identical instructions
https://godbolt.org/z/rVLBcf 

- You can use GVN pass (include/llvm/Transforms/Scalar/GVN.h)

#### 15. Arithmetic optimizations
- There is InstCombinePass (include/llvm/Transforms/InstCombine/InstCombine.h)
- Naively using this will work in some cases but not in general, because our cost model is inversed (multiply is cheaper than add)
- You can implement by yourself using matchers.
- Don’t copy & paste patterns!!

#### 16. Dead argument elimination
https://godbolt.org/z/2a8M72  

- You can use DeadArgumentEliminationPass (include/llvm/Transforms/IPO/DeadArgumentElimination.h)
- You’ll need to attach keyword ‘internal’ to functions.

#### 17. Loop Interchange (column-major access -> row-major access)

- LoopInterchange.cpp implements this.
