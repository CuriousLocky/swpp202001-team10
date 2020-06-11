#include "Alloca2reg.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
using namespace llvm;
using namespace std;

  PreservedAnalyses Alloca2reg::run(Function &F, FunctionAnalysisManager &FAM) {
    DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
    vector<AllocaInst *> Allocas;
    for (auto &BB : F){
      for (auto &I : BB){
        if(I.getOpcode() == Instruction::MemoryOps::Alloca){
          AllocaInst* AI = dyn_cast<AllocaInst>(&I);
          if(isAllocaPromotable(AI)){
            Allocas.push_back(AI);
          }
        }

      }
    }
	  PromoteMemToReg(Allocas, DT, nullptr);
    return PreservedAnalyses::all();
  }
