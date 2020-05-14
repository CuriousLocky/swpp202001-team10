#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include <vector>

using namespace llvm;
using namespace std;
using namespace llvm::PatternMatch;

namespace {
class MallocToAlloca : public PassInfoMixin<MallocToAlloca> {
  public: PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
  // Going through all instructions to find all malloc's that have been freed before function end  
    for (auto &BB : F)  
      for (auto &I : BB)
        if (auto *CB = dyn_cast<CallBase>(&I)) 
          if (CB->getCalledFunction()->getName() == "malloc")                   // current instruction is malloc call
            for (auto itr = I.use_begin(), end = I.use_end(); itr != end;) {    // see all uses of this malloc variable
              Use &U = *itr++;
              User *Usr = U.getUser();
              
              if (Instruction *UsrI = dyn_cast<Instruction>(Usr)) 
                if(CallInst *CI = dyn_cast<CallInst>(UsrI)) 
                  if (CI->getCalledFunction()->getName() == "free") {           // current inst is free -> malloc is freed
                    Function *m = cast <Function> (I.getOperand(1));
                    Type *t;
                    for (auto &Arg : m -> args()) {
                      t = Arg.getType();                                        // to know type of malloc variable
                    }

                    unsigned int *add = (unsigned int *) malloc(sizeof(unsigned int));
                    AllocaInst* alloc = new AllocaInst(t, *add, I.getName(), &BB);
                    I.replaceAllUsesWith(alloc);
                    UsrI -> eraseFromParent();
                  }  
            }

    return PreservedAnalyses::all();
  }
};
}

extern "C" ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "MallocToAlloca", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "malloc-to-alloca") {
            FPM.addPass(MallocToAlloca());
            return true;
          }
          return false;
        }
      );
    }
  };
}


