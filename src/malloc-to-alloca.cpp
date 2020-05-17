#include "SimpleBackend.h"

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
#include <vector>

using namespace llvm;
using namespace std;
using namespace llvm::PatternMatch;

PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
  vector <Instruction *> inst_to_change;
  vector <Instruction *> inst_to_remove;
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
                    inst_to_remove.push_back(&UsrI);
                  }  
            }
    // Changing malloc to alloca
      for (auto *I: inst_to_change) {
        
        Function *m = cast <Function> (I -> getOperand(1));
        Type *t;
        for (auto &Arg : m -> args()) {
          t = Arg.getType();                                        // to know type of malloc variable
        }
        outs() << I -> getName() << '\n';
      /*
        unsigned int *add = (unsigned int *) malloc(sizeof(unsigned int));
        AllocaInst* alloc = new AllocaInst(t, *add, I->getName(), &BB);
      */
        BasicBlock *parent = I -> getParent();
        IRBuilder<> ParentBuilder(parent);
        auto *alloc = ParentBuilder.CreateAlloca(t, nullptr, I->getName());  
        I -> replaceAllUsesWith(alloc);
      }
    // Remove free instructions
      for (auto *I: inst_to_remove) {
        I -> eraseFromParent();
      }
    return PreservedAnalyses::all();
  }
