#ifndef ArithmeticOptimization_H
#define ArithmeticOptimization_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

class ArithmeticOptimization : public PassInfoMixin<ArithmeticOptimization>{
 public:
  llvm::PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif
