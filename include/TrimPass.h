#ifndef TrimPass_H
#define TrimPass_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

class TrimPass : public PassInfoMixin<TrimPass> {
 public:
  llvm::PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif