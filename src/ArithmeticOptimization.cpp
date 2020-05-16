#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;
using namespace std;

namespace {
class ArithmeticOptimization : public PassInfoMixin<ArithmeticOptimization> {

public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
	vector<Instruction *> removableInst;

		for (auto &BB : F) {
      for (auto &I : BB) {

        BinaryOperator * Inst;
        ConstantInt* C;
        uint64_t a;
        
        switch(I.getOpcode()){
          
          case Instruction::Shl:
            // assuming 0 << 0 won't occure since it's been already removed by
            // constant folding opt
            // X << 0
            C = dyn_cast<ConstantInt>(I.getOperand(1));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
                break;
              }
              // X << constantInt(non-zero)
              else{
                auto c1 = ConstantInt::get(I.getOperand(1)->getType(), pow(2.0, a), false);
                Inst = BinaryOperator::Create(BinaryOperator::Mul, I.getOperand(0), c1,
                    "inst1", &I);
                I.replaceAllUsesWith(Inst);
                removableInst.push_back(&I);
                break;
              }
            }
            // 0 << X
            C = dyn_cast<ConstantInt>(I.getOperand(0));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
              }
            }
            break;
          //case Instruction::LShr:
          case Instruction::AShr: 
            // assuming 0 >> 0 won't occure since it's been already removed by
            // constant folding opt
            // X >> 0
            C = dyn_cast<ConstantInt>(I.getOperand(1));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
                break;
              }
              // X >> constantInt(non-zero)
              else{
                auto c1 = ConstantInt::get(I.getOperand(1)->getType(), pow(2.0, a), false);
                Inst = BinaryOperator::Create(BinaryOperator::UDiv, I.getOperand(0), c1,
                    "inst2", &I);
                I.replaceAllUsesWith(Inst);
                removableInst.push_back(&I);
                break;
              }
            }
            // 0 >> X
            C = dyn_cast<ConstantInt>(I.getOperand(0));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
              }
            }
            break;

          case Instruction::Add:
            // assuming 0 + 0 won't occure since it's been already removed by
            // constant folding opt
            // X + X
            if(I.getOperand(0) == I.getOperand(1)){
              C = ConstantInt::get( I.getOperand(0)->getContext(), APInt(32, StringRef("2"), 10));
              Inst = BinaryOperator::Create(BinaryOperator::Mul, I.getOperand(0), C,
                    "inst3", &I);
              I.replaceAllUsesWith(Inst);
              removableInst.push_back(&I);
              break;
            }
            // 0 + X
            C = dyn_cast<ConstantInt>(I.getOperand(0)); 
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(1));
                removableInst.push_back(&I);
                break;
              }
            }
            // X + 0
            C = dyn_cast<ConstantInt>(I.getOperand(1));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
              }
            } 
            break;

          case Instruction::Sub:
            // assuming 0 - 0 won't occure since it's been already removed by
            // constant folding opt
            // X - X
            if(I.getOperand(0) == I.getOperand(1)){
              C  = ConstantInt::get( I.getOperand(0)->getContext(), APInt(32, StringRef("0"), 10));
              I.replaceAllUsesWith(C);
              removableInst.push_back(&I);
              break;
            }
            // 0 - X
            C = dyn_cast<ConstantInt>(I.getOperand(0));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                C  = ConstantInt::get( I.getOperand(1)->getContext(), APInt(32, StringRef("-1"), 10));
                Inst = BinaryOperator::Create(BinaryOperator::Mul, I.getOperand(1), C,
                    "inst4", &I);
                I.replaceAllUsesWith(Inst);
                removableInst.push_back(&I);
                break;
              }
            }
            // X - 0
            C = dyn_cast<ConstantInt>(I.getOperand(1));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
              }
            }
            break;

            case Instruction::SDiv:
            case Instruction::UDiv:

              // X / X
              if(I.getOperand(0) == I.getOperand(1)){
              C  = ConstantInt::get( I.getOperand(0)->getContext(), APInt(32, StringRef("1"), 10));
              I.replaceAllUsesWith(C);
              removableInst.push_back(&I);
              break;
            }

            // 0 / X
            C = dyn_cast<ConstantInt>(I.getOperand(0));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 0){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
                break;
              }
            }
            

            // X / 1
            C = dyn_cast<ConstantInt>(I.getOperand(1));
            if(C != nullptr){
              a = C->getZExtValue();
              if(a == 1){
                I.replaceAllUsesWith(I.getOperand(0));
                removableInst.push_back(&I);
              }
            }
            break;

            case Instruction::SRem:
            case Instruction::URem:
              
              // X % X
              if(I.getOperand(0) == I.getOperand(1)){
              C  = ConstantInt::get( I.getOperand(0)->getContext(), APInt(32, StringRef("0"), 10));
              I.replaceAllUsesWith(C);
              removableInst.push_back(&I);
              break;
            }
              // 0 % X
              C = dyn_cast<ConstantInt>(I.getOperand(0));
              if(C != nullptr){
                a = C->getZExtValue();
                if(a == 0){
                  I.replaceAllUsesWith(I.getOperand(0));
                  removableInst.push_back(&I);
                  break;
                }
              }
              
              // X % 1
              C = dyn_cast<ConstantInt>(I.getOperand(1));
              if(C != nullptr){
                a = C->getZExtValue();
                if(a == 1){
                  C  = ConstantInt::get( I.getOperand(0)->getContext(), APInt(32, StringRef("0"), 10));
                  I.replaceAllUsesWith(C);
                  removableInst.push_back(&I);
                }
              }
              break;
            case Instruction::Or:
              // X | X
              if(I.getOperand(0) == I.getOperand(1)){
              I.replaceAllUsesWith(I.getOperand(0));
              removableInst.push_back(&I);
              break;
            }
              // 0 | X
              C = dyn_cast<ConstantInt>(I.getOperand(0)); 
              if(C != nullptr){
                a = C->getZExtValue();
                if(a == 0){
                  I.replaceAllUsesWith(I.getOperand(1));
                  removableInst.push_back(&I);
                  break;
                }
              }
            
              // X | 0
              C = dyn_cast<ConstantInt>(I.getOperand(1)); 
              if(C != nullptr){
                a = C->getZExtValue();
                if(a == 0){
                  I.replaceAllUsesWith(I.getOperand(0));
                  removableInst.push_back(&I);
                  
                }
              }
              break;
            case Instruction::And:
              // X & X
              if(I.getOperand(0) == I.getOperand(1)){
              I.replaceAllUsesWith(I.getOperand(0));
              removableInst.push_back(&I);
              break;
              }
              // 0 & X
              C = dyn_cast<ConstantInt>(I.getOperand(0)); 
              if(C != nullptr){
                a = C->getZExtValue();
                if(a == 0){
                  I.replaceAllUsesWith(I.getOperand(0));
                  removableInst.push_back(&I);
                  break;
                }
              }
            
              // X & 0
              C = dyn_cast<ConstantInt>(I.getOperand(1)); 
              if(C != nullptr){
                a = C->getZExtValue();
                if(a == 0){
                  I.replaceAllUsesWith(I.getOperand(1));
                  removableInst.push_back(&I);
                  
                }
              }
              break;

            case Instruction::Xor:
             // X ^ X
             if(I.getOperand(0) == I.getOperand(1)){
              C  = ConstantInt::get( I.getOperand(0)->getContext(), APInt(32, StringRef("0"), 10));
              I.replaceAllUsesWith(C);
              removableInst.push_back(&I);
              }
            break;
        }
	  	}
    }
    for(auto *I : removableInst)
      I->eraseFromParent();
    return PreservedAnalyses::all();
  }
};
}



