#ifndef ArithmeticOptimization_H
#define ArithmeticOptimization_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;
using namespace std;


PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);

#endif
