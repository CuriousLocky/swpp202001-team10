#ifndef MallocToAllocaOpt_H
#define MallocToAllocaOpt_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

class MallocToAllocaOpt : public PassInfoMixin<MallocToAllocaOpt>{
 public:
  llvm::PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif
