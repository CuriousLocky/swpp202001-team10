#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
// feel free to add more if you need them

#include "Example.h"

using namespace llvm;
using namespace std;

//static void something(){} //static functions if needed

PreservedAnalyses Example::run(Module &M, FunctionAnalysisManager &FAM) {
    // what you want to do
    return PreservedAnalyses::all();
}