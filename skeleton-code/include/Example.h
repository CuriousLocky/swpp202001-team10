#ifndef Example_H
#define Example_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;
//using namespace std //if needed

class Example : public PassInfoMixin<Example>{
private:
  //some methods or members if needed
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif