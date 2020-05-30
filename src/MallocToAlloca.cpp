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

using namespace llvm;
using namespace std;
using namespace llvm::PatternMatch;

PreservedAnalyses MallocToAllocaOpt::run(Function &F, FunctionAnalysisManager &FAM) {
  vector <Instruction *> inst_to_change;
  vector <Instruction *> inst_to_remove;
  LLVMContext &Context = F.getContext();
  //DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
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
                  inst_to_change.push_back(&I);
                  inst_to_remove.push_back(UsrI);
                  inst_to_remove.push_back(&I);
                  break;
                }  
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
    
    IRBuilder<> ParentBuilder(I);
    ConstantInt* size = ConstantInt::get(IntegerType::getInt32Ty(Context), num);
    auto *alloc = ParentBuilder.CreateAlloca(IntegerType::getInt8Ty(Context), size , "alloca_" + I->getName());
    I->replaceAllUsesWith(alloc);
  /*  
  // Create an array type of i8, because output of malloc is i8    
    ArrayType* arrayType = ArrayType::get(IntegerType::getInt8Ty(Context), num);
  // Insert alloca before malloc
    IRBuilder<> ParentBuilder(I);
    auto *alloc = ParentBuilder.CreateAlloca(arrayType, nullptr, "alloc");
  // Get pointer to first element
    Value* idxList[2] = {ConstantInt::get(IntegerType::getInt32Ty(Context), 0), ConstantInt::get(IntegerType::getInt32Ty(Context), 0)};
    GetElementPtrInst* gepInst = GetElementPtrInst::Create(arrayType, alloc, ArrayRef<Value*>(idxList, 2), I->getName(), I);
    outs() << *gepInst->getType() << ' ' << *I->getType() << '\n';
  // Replace malloc instruction  
    I->replaceAllUsesWith(gepInst);*/
  }
// Remove malloc and free instructions
  for (auto *I: inst_to_remove) {
    I->removeFromParent();
  }
  for (auto &BB : F) {
    for (Instruction &I : BB)
      outs() << "\t" << I << "\n";
  }
  return PreservedAnalyses::all();
}
