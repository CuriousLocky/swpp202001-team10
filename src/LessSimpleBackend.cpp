#include "llvm/Analysis/TargetFolder.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_os_ostream.h"
#include <string>
#include <sstream>
#include <memory>

#include "LessSimpleBackend.h"

using namespace llvm;
using namespace std;

// Return sizeof(T) in bytes.
unsigned getAccessSize(Type *T) {
  if (isa<PointerType>(T))
    return 8;
  else if (isa<IntegerType>(T)) {
    return T->getIntegerBitWidth() == 1 ? 1 : (T->getIntegerBitWidth() / 8);
  } else if (isa<ArrayType>(T)) {
    return getAccessSize(T->getArrayElementType()) * T->getArrayNumElements();
  }
  assert(false && "Unsupported access size type!");
}

// A simple namer. :) 
// Borrowed from SimpleBackend
class InstNamer : public InstVisitor<InstNamer> {
public:
  void visitFunction(Function &F) {
    for (auto &Arg : F.args())
      if (!Arg.hasName())
        Arg.setName("arg");

    for (BasicBlock &BB : F) {
      if (!BB.hasName())
        BB.setName(&F.getEntryBlock() == &BB ? "entry" : "bb");

      for (Instruction &I : BB)
        if (!I.hasName() && !I.getType()->isVoidTy())
          I.setName("tmp");
    }
  }
};

class ConstExprToInsts : public InstVisitor<ConstExprToInsts> {
  Instruction *ConvertCEToInst(ConstantExpr *CE, Instruction *InsertBefore) {
    auto *NewI = CE->getAsInstruction();
    NewI->insertBefore(InsertBefore);
    NewI->setName("from_constexpr");
    visitInstruction(*NewI);
    return NewI;
  }
public:
  void visitInstruction(Instruction &I) {
    for (unsigned Idx = 0; Idx < I.getNumOperands(); ++Idx) {
      if (auto *CE = dyn_cast<ConstantExpr>(I.getOperand(Idx))) {
        I.setOperand(Idx, ConvertCEToInst(CE, &I));
      }
    }
  }
};

class StackFrame{
    vector<pair<Instruction*, int>> frame;
    Function &F;
    int maxStackSize = 0;
    bool usedAfter(Instruction &I, Instruction &I_checked){
        DominatorTree DT(F);
        for(auto *User : I.users()){
            if(Instruction *UserI = dyn_cast<Instruction>(User)){
                if(!DT.dominates(UserI, &I_checked)){
                    return true;
                }
            }
        }
        return false;
    }
    void tryDumpRedundant(Instruction &I_current){
        for(auto content : frame){
            if(content.first == nullptr){
                continue;
            }
            if(!usedAfter(*content.first, I_current)){
                content.first = nullptr;
            }
        }
        for(int i = 1; i < frame.size(); i++){
            auto content = frame[i];
            if(content.first == nullptr && 
                frame[i-1].first == nullptr){
                frame[i] = pair(nullptr, content.second+frame[i-1].second);
                frame[i-1] = pair(nullptr, 0);
            }
        }
        for(auto content = frame.begin(); content != frame.end(); content++){
            if(content->second == 0){
                frame.erase(content);
            }
        }
    }
public:
    StackFrame(Function &F_in):F(F_in){}
    int putOnStack(Instruction& I){
        tryDumpRedundant(I);
        int size = getAccessSize(I.getType());
        bool onStackFlag = false;
        int offset = 0;
        for(int i = 0; i < frame.size(); i++){
            auto content = frame[i];
            if(content.first==nullptr){
                if(content.second==size){
                    frame[i] = pair(&I, size);
                }
                if(content.second>size){
                    frame[i] = pair(&I, size);
                    frame.insert(frame.begin()+i, pair(nullptr, content.second-size));
                }
                onStackFlag = true;
                break;
            }
            offset += content.second;
        }
        if(!onStackFlag){
            auto new_content = pair(&I, size);
            frame.push_back(new_content);            
        }
        if(maxStackSize < offset + size){
            maxStackSize = offset + size;
        }
        return offset;
    }
    int findOnStack(Instruction& I){
        int offset = 0;
        for(int i = 0; i < frame.size(); i++){
            if(frame[i].first == &I){
                return offset;
            }
            offset += frame[i].second;
        }
        assert(false);
        return -1;
    }
};

class Registers{
    vector<Instruction*> regs;
    Function &F;
    bool usedAfter(Instruction &I, Instruction &I_checked){
        DominatorTree DT(F);
        
    }
    void tryDumpRedundant(Instruction &I_current){
        for(int i = 0; i < regs.size(); i++){
            if(regs[i]==nullptr){
                continue;
            }
            if(!usedAfter(*regs[i], I_current)){
                regs[i] = nullptr;
            }
        }
    }
public:
    Registers(Function &F):F(F),regs(vector<Instruction*>(16)){}
    int putOnRegs(Instruction* I){

    }
};

static void renameArg(Argument &arg, int num){
    arg.setName("__arg__" + num);
}

static void depromoteReg_BB(BasicBlock &BB, vector<Value*> &regs, StackFrame &frame){
    for(Instruction &I : BB){
        int 
    }
}

static void depromoteReg(Function &F){
    for(int i = 0; i < F.arg_size(); i++){
        renameArg(*F.getArg(i), i);
    }
    vector<Value*> regs(16);
    for(int i = 0; i < 16; i++){
        regs[i] = nullptr;
    }
    StackFrame frame(F);

}

PreservedAnalyses LessSimpleBackend::run(Module &M, ModuleAnalysisManager &MAM){
    if (verifyModule(M, &errs(), nullptr)){
        exit(1);
    }

    // First, name all instructions / arguments / etc.
    InstNamer Namer;
    Namer.visit(M);

    // Second, convert known constant expressions to instructions.
    ConstExprToInsts CEI;
    CEI.visit(M);

    for(Function &F : M.getFunctionList()){
        if(!F.isDeclaration()){
            depromoteReg(F);
        }
    }
    
    return PreservedAnalyses::all();
}