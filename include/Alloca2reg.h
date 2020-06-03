#ifndef Alloca2reg_H
#define Alloca2reg_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

class Alloca2reg : public PassInfoMixin<Alloca2reg>{
 public:
  llvm::PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif
