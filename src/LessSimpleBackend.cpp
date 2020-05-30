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
static unsigned getAccessSize(Type *T) {
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

class LessSimpleBackend::StackFrame{
    vector<pair<Instruction*, int>> frame;
    Function *F;
    LessSimpleBackend *Backend;
    int maxStackSize = 0;
    bool usedAfter(Instruction *I, Instruction *I_checked){
        DominatorTree DT(*F);
        for(auto *User : I->users()){
            if(Instruction *UserI = dyn_cast<Instruction>(User)){
                if(UserI==I_checked || !DT.dominates(UserI, I_checked)){
                    return true;
                }
            }
        }
        return false;
    }
    void mergeEmpty(){
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
    void tryDumpRedundant(Instruction *I_current){
        for(int i = 0; i < frame.size(); i++){
            if(frame[i].first == nullptr){
                continue;
            }
            if(!usedAfter(frame[i].first, I_current)){
                frame[i].first = nullptr;
            }            
        }
        mergeEmpty();
    }
public:
    StackFrame(Function *F_in, LessSimpleBackend *Backend):
        F(F_in), Backend(Backend){}
    int putOnStack(Instruction* I){return putOnStack(I, I);}
    int putOnStack(Instruction* I, Instruction *I_current){
        int offset = findOnStack(I);
        if(offset < 0){
            offset = 0;
        }else{
            return offset;
        }
        tryDumpRedundant(I_current);
        int size = getAccessSize(I->getType());
        bool onStackFlag = false;
        for(int i = 0; i < frame.size(); i++){
            auto content = frame[i];
            if(content.first==nullptr){
                if(content.second==size){
                    frame[i] = pair(I, size);
                }
                if(content.second>size){
                    frame[i] = pair(I, size);
                    frame.insert(frame.begin()+i, pair(nullptr, content.second-size));
                }
                onStackFlag = true;
                break;
            }
            offset += content.second;
        }
        if(!onStackFlag){
            auto new_content = pair(I, size);
            frame.push_back(new_content);            
        }
        if(maxStackSize < offset + size){
            maxStackSize = offset + size;
        }
        return offset;
    }
    int findOnStack(Instruction* I){
        int offset = 0;
        for(int i = 0; i < frame.size(); i++){
            if(frame[i].first == I){
                return offset;
            }
            offset += frame[i].second;
        }
        return -1;
    }
    int removeFromStack(Instruction* I){
        int offset = 0;
        for(int i = 0; i < frame.size(); i++){
            if(frame[i].first == I){
                frame[i].first = nullptr;
                break;
            }
            offset+=frame[i].second;
        }
        mergeEmpty();
        return offset;
    }
    void printFrame(){
        int offset = 0;
        for(auto iter : frame){
            string eleName = "nullptr";
            if(iter.first!=nullptr){
                eleName = iter.first->getName();
            }
            outs()<<"{"+eleName+"\t"+to_string(offset+iter.second)+"}";
            offset += iter.second;
        }
        outs()<<"\n\n";
    }
    void replaceWith(Instruction *oldInst, Instruction *newInst){
        for(int i = 0; i < frame.size(); i++){
            if(frame[i].first == oldInst){
                frame[i].first = newInst;
                return;
            }
        }
    }
};

class LessSimpleBackend::Registers{
    vector<Instruction*> regs;
    Function *F;
    LessSimpleBackend *Backend;
public:
    Instruction* getInst(int regPos){
        assert(regPos>0 && regPos<=REG_SIZE);
        return regs[regPos-1];
    }
    void setInst(Instruction *I, int regPos){
        assert(regPos>0 && regPos<=REG_SIZE);
        regs[regPos-1] = I;
    }
    bool usedAfter(Instruction *I, Instruction *I_checked){
        DominatorTree DT(*F);
        for(auto *User : I->users()){
            if(Instruction *UserI = dyn_cast<Instruction>(User)){
                if(UserI==I_checked || !DT.dominates(UserI, I_checked)){
                    return true;
                }
            }
        }
        return false;
    }
    void tryDumpRedundant(Instruction *I_current){
        for(int i = 0; i < regs.size(); i++){
            if(regs[i]==nullptr){
                continue;
            }
            if(!usedAfter(regs[i], I_current)){
                regs[i] = nullptr;
            }
        }
    }
    int findVictimExcept(vector<int> exceptPosList){
        for(int i = 0; i < regs.size(); i++){
            bool canBeVictim = true;
            for(int j = 0; j < exceptPosList.size(); j++){
                if(exceptPosList[j] == i){
                    canBeVictim = false;
                    break;
                }
            }
            if(canBeVictim){
                return i+1;
            }
        }
        return -1;
    }
    Instruction *loadToReg(Instruction *IOnStack, StackFrame *frame, 
        Instruction *InsertBefore, int regNum){
        int offset = frame->findOnStack(IOnStack);
        IRBuilder<> Builder(InsertBefore);
        CallInst *temp_p = Builder.CreateCall(
            Backend->getSpOffset(),
            {ConstantInt::getSigned(IntegerType::getInt64Ty(IOnStack->getContext()), offset)},
            "temp_p_"+IOnStack->getName()
        );
        Value *loadOperand = temp_p;
        if(!(IOnStack->getType()->isIntegerTy() &&
            IOnStack->getType()->getIntegerBitWidth()==8
            )){
            loadOperand = Builder.CreateBitCast(
                temp_p, IOnStack->getType()->getPointerTo(), 
                temp_p->getName()+"_cast"
            );
        }
        Instruction *newInst = Builder.CreateLoad(
            loadOperand,
            genRegName(IOnStack, regNum)
        );
        regs[regNum-1] = newInst;
        return newInst;
    }
    void storeToFrame(Instruction *IOnReg, StackFrame *frame,
        Instruction *InsertBefore, int regNum){
        if(regNum <= 0){
            regNum = findOnRegs(IOnReg);
            assert(regNum>0);
        }
        int offset = frame->putOnStack(IOnReg, InsertBefore);
        IRBuilder<> Builder(InsertBefore);
        CallInst *temp_p = Builder.CreateCall(
            Backend->getSpOffset(),
            {ConstantInt::getSigned(IntegerType::getInt64Ty(IOnReg->getContext()), offset)},
            "temp_p_"+IOnReg->getName()
        );
        Value *loadOperand = temp_p;
        if(!(IOnReg->getType()->isIntegerTy() &&
            IOnReg->getType()->getIntegerBitWidth()==8
            )){
            loadOperand = Builder.CreateBitCast(
                temp_p, IOnReg->getType()->getPointerTo(), 
                temp_p->getName()+"_cast"
            );
        }
        Builder.CreateStore(
            IOnReg,
            loadOperand
        );
        regs[regNum-1] = nullptr;
    }
    string genRegName(Instruction *I, int regNum){
        return "r"+to_string(regNum)+"_"+I->getName().str();
    }

    Registers(Function *F, LessSimpleBackend *Backend):
        F(F),Backend(Backend),regs(vector<Instruction*>(REG_SIZE)){
        for(int i = 0; i < regs.size(); i++){
            regs[i] = nullptr;
        }
    }
    int tryPutOnRegs(Instruction* I){
        return tryPutOnRegs(I, I);
    }
    int tryPutOnRegs(Instruction* I, Instruction* I_current){
        tryDumpRedundant(I_current);
        for(int i = 0; i < regs.size(); i++){
            if(regs[i] == nullptr){
                regs[i] = I;
                return i+1;
            }
        }
        return -1;
    }
    int findOnRegs(Instruction* I){
        for(int i = 0; i < regs.size(); i++){
            if(regs[i] == I){
                return i+1;
            }
        }
        return -1;
    }
    void printRegs(){
        for(int i = 0; i < regs.size(); i++){
            string regName = "nullptr";
            if(regs[i]!=nullptr){
                regName = regs[i]->getName();
            }
            outs()<<"r"+to_string(i+1)+"\t"+regName+"\n";
        }
        outs()<<"\n";
    }
    void replaceWith(Instruction *oldInst, Instruction *newInst){
        for(int i = 0; i < regs.size(); i++){
            if(regs[i] == oldInst){
                regs[i] = newInst;
            }
        }
    }
};

static string getUniqueName(string nameBase, Module &M){
    for(Function &F : M){
        if(F.getName() == nameBase){
            return getUniqueName(nameBase+"_", M);
        }
    }
    return nameBase;
} 

static void renameArg(Argument &arg, int num){
    arg.setName("__arg__" + to_string(num));
}

Function *LessSimpleBackend::getSpOffset(){return spOffset;}
Function *LessSimpleBackend::getRstH(){return rstH;}
Function *LessSimpleBackend::getRstS(){return rstS;}

void LessSimpleBackend::loadOperands(
    Instruction *I, vector<pair<Instruction*, int>> &evicRegs, 
    vector<int> &operandOnRegs){
        vector<Instruction*> toMoveToRegs;
        IRBuilder<> Builder(I);
        for(int i = 0; i < I->getNumOperands(); i++){
            Value* operand = I->getOperand(i);
            if(Instruction* operand_I = dyn_cast<Instruction>(operand)){
                int regNum = regs->findOnRegs(operand_I);
                if(regNum > 0){
                    operandOnRegs.push_back(regNum-1);
                    continue;
                }
                int stackOffset = frame->findOnStack(operand_I);
                if(stackOffset >= 0){
                    toMoveToRegs.push_back(operand_I);
                    continue;
                }
            }
        }
        for(Instruction* IOnStack : toMoveToRegs){
            int victimRegNum=regs->tryPutOnRegs(IOnStack, I);
            if(victimRegNum <= 0){
                victimRegNum = regs->findVictimExcept(operandOnRegs);
                evicRegs.push_back(pair(regs->getInst(victimRegNum), victimRegNum));
                regs->storeToFrame(regs->getInst(victimRegNum), frame, I, victimRegNum);
            }
            Instruction* newInst = regs->loadToReg(IOnStack, frame, I, victimRegNum);
            I->replaceUsesOfWith(IOnStack, newInst);
            operandOnRegs.push_back(victimRegNum-1);
        }
}

bool LessSimpleBackend::putOnRegs(
    Instruction *I, vector<pair<Instruction*, int>> &evicRegs,
    vector<int> &operandOnRegs){
    Instruction *I_next = I->getNextNode();
    int victimRegNum = regs->tryPutOnRegs(I);
    bool dumpFlag = false;
    if(victimRegNum <= 0){
        victimRegNum = regs->findVictimExcept(operandOnRegs);
        evicRegs.push_back(pair(regs->getInst(victimRegNum), victimRegNum));
        regs->storeToFrame(regs->getInst(victimRegNum), frame, I, victimRegNum);
        dumpFlag = true;
    }
    I->setName(regs->genRegName(I, victimRegNum));
    regs->setInst(I, victimRegNum);
    return dumpFlag;
}

void LessSimpleBackend::resumeRegs(
    Instruction *I, vector<pair<Instruction*, int>> &evicRegs,
    bool dumpFlag){
    Instruction *I_next = I->getNextNode();
    if(dumpFlag){
        regs->storeToFrame(I, frame, I_next, 0);
    }
    for(int i = evicRegs.size()-1; i >= 0; i--){
        Instruction *IOnStack = evicRegs[i].first;
        int regPos = evicRegs[i].second;
        regs->loadToReg(IOnStack, frame, I_next, regPos);
        DominatorTree DT(*I->getFunction());
        for(BasicBlock &BB : *I->getFunction()){
            for(Instruction &Inst : BB){
                if(DT.dominates(regs->getInst(regPos), &Inst)){
                    Inst.replaceUsesOfWith(IOnStack, regs->getInst(regPos));
                }
            }
        }
        frame->replaceWith(IOnStack, regs->getInst(regPos));
    }
}

void LessSimpleBackend::depromoteReg_BB(BasicBlock &BB){
    vector<Instruction*> instList;
    for(Instruction &I : BB){
        instList.push_back(&I);
    }
    for(Instruction *I : instList){
        if(!I->isTerminator()){
            vector<pair<Instruction*, int>> evicRegs;
            vector<int> operandOnRegs;
            loadOperands(I, evicRegs, operandOnRegs);
            bool dumpFlag = false;
            if(I->hasName()){
                dumpFlag = putOnRegs(I, evicRegs, operandOnRegs);
            }
            resumeRegs(I, evicRegs, dumpFlag);
        }
    }
    
}

void LessSimpleBackend::depromoteReg(Function &F){
    for(int i = 0; i < F.arg_size(); i++){
        renameArg(*F.getArg(i), i);
    }
    regs = new LessSimpleBackend::Registers(&F, this);
    frame = new LessSimpleBackend::StackFrame(&F, this);
    for(BasicBlock &B : F){
        depromoteReg_BB(B);
    }
    delete(regs);
    delete(frame);
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

    string rstHName = getUniqueName("__resetHeap", M);
    string rstSName = getUniqueName("__resetStack", M);
    string spOffsetName = getUniqueName("__spOffset", M);

    Type *VoidTy = Type::getVoidTy(M.getContext());
    PointerType *I8PtrTy = PointerType::getInt8PtrTy(M.getContext());
    IntegerType *I64Ty = IntegerType::getInt64Ty(M.getContext());

    FunctionType *rstTy = FunctionType::get(VoidTy, {VoidTy}, false);
    FunctionType *spOffsetTy = FunctionType::get(I8PtrTy, {I64Ty}, false);

    rstH = Function::Create(rstTy, Function::ExternalLinkage, rstHName, M);
    rstS = Function::Create(rstTy, Function::ExternalLinkage, rstSName, M);
    spOffset = Function::Create(spOffsetTy, Function::ExternalLinkage, spOffsetName, M);

    for(Function &F : M.getFunctionList()){
        if(!F.isDeclaration()){
            depromoteReg(F);
        }
    }

    outs()<<M;
    
    return PreservedAnalyses::all();
}