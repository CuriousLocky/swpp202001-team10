#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "MallocToAllocaOpt.h"
#include <vector>
#include <map>

using namespace llvm;
using namespace std;
using namespace llvm::PatternMatch;

vector <Instruction *> inst_to_change;
vector <Instruction *> inst_to_remove;
vector <Instruction *> used;
//map <Instruction *, bool> m;

bool find_free(Instruction *I) {
  //m[I] = true;
  used.push_back(I);
  for (auto itr = I->use_begin(), end = I->use_end(); itr != end;) {    // see all uses of this malloc variable
    Use &U = *itr++;
    User *Usr = U.getUser();
    if (Instruction *UsrI = dyn_cast<Instruction>(Usr)) {
      if(CallInst *CI = dyn_cast<CallInst>(UsrI)) {
        if (CI->getCalledFunction()->getName() == "free") {           // current inst is free -> malloc is freed
          inst_to_remove.push_back(UsrI);
          return true; 
        }
      }

      if (isa<StoreInst>(UsrI)) { 
        bool flag = true;
        for (int i = 0; i < used.size(); ++i)
          if (used[i] == dyn_cast<Instruction>(UsrI->getOperand(1)))
            flag = false;
          
        if (flag)  
          return find_free(dyn_cast<Instruction>(UsrI->getOperand(1)));
      }

      if (isa<BitCastInst> (UsrI) || isa<LoadInst>(UsrI)) {
        bool flag = true;
        for (int i = 0; i < used.size(); ++i)
          if (used[i] == UsrI)
            flag = false;

        if (flag)
          return find_free(UsrI);  
      }
    }
  }
  return false;
}

PreservedAnalyses MallocToAllocaOpt::run(Function &F, FunctionAnalysisManager &FAM) {
  LLVMContext &Context = F.getContext();
// Going through all instructions to find all malloc's that have been freed before function end  
  for (auto &BB : F)  
    for (auto &I : BB)
      if (auto *CB = dyn_cast<CallBase>(&I)) 
        if (CB->getCalledFunction()->getName() == "malloc")                   // current instruction is malloc call
          if (find_free(&I)) {
            inst_to_change.push_back(&I);
            inst_to_remove.push_back(&I);
          }

// Changing malloc to alloca
  for (auto *I: inst_to_change) {
  // Find how many bytes are allocated  
    Value *val = (dyn_cast<CallBase>(I)) -> getOperand(0);                
    int num = 0;
    if (ConstantInt* CI = dyn_cast<ConstantInt>(val)) 
      num = CI->getSExtValue();
    if (num > 256)                // check size of request memory, should be less than 256 for now
      continue;  
    //outs() << *I << '\n'; 
    IRBuilder<> ParentBuilder(I);
    ConstantInt* size = ConstantInt::get(IntegerType::getInt32Ty(Context), num);
    auto *alloc = ParentBuilder.CreateAlloca(IntegerType::getInt8Ty(Context), size , "alloca_" + I->getName());
    I->replaceAllUsesWith(alloc);
  }

// Remove malloc and free instructions
  for (auto *I: inst_to_remove) {
    I->removeFromParent();
  }
// Print result  
  for (auto &BB : F) {
    for (Instruction &I : BB)
      outs() << "\t" << I << "\n";
  }
  return PreservedAnalyses::all();
}
