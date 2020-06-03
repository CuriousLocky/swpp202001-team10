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
    int putOnStack(Instruction* I, bool dumpFlag=true, int size=-1){return putOnStack(I, I, dumpFlag, size);}
    int putOnStack(Instruction* I, Instruction *I_current, bool dumpFlag=true, int size=-1){
        int offset = findOnStack(I);
        if(offset < 0){
            offset = 0;
        }else{
            return offset;
        }
        if(dumpFlag){
            tryDumpRedundant(I_current);            
        }
        if(size < 0){
            size = getAccessSize(I->getType());
        }
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
            Backend->getTempPrefix()+IOnStack->getName()
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
            Backend->getTempPrefix()+IOnReg->getName()
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

static string getUniqueFnName(string nameBase, Module &M){
    for(Function &F : M){
        if(F.getName() == nameBase){
            return getUniqueFnName(nameBase+"_", M);
        }
    }
    return nameBase;
} 

static string getUniquePrefix(string nameBase, Module &M){
    for(Function &F : M){
        for(BasicBlock &BB : F){
            for(Instruction &I : BB){
                if(I.getName().startswith(nameBase)){
                    return getUniquePrefix("_"+nameBase, M);
                }
            }
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
string LessSimpleBackend::getTempPrefix(){return tempPrefix;}

void LessSimpleBackend::loadOperands(
    Instruction *I, vector<pair<Instruction*, int>> &evicRegs, 
    vector<int> &operandOnRegs){
        vector<Instruction*> toMoveToRegs;
        IRBuilder<> Builder(I);
        for(int i = 0; i < I->getNumOperands(); i++){
            Value* operand = I->getOperand(i);
            if(Instruction* operand_I = dyn_cast<Instruction>(operand)){
                if(operand->getName().startswith(tempPrefix)){
                    continue;
                }
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
    bool dumpFlag=false){
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

void LessSimpleBackend::regAlloc(Function& F){
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            Instruction *I_p = &I;
            vector<pair<Instruction*, int>> evicRegs;
            vector<int> operandOnRegs;
            bool dumpFlag = false;
            if(loadOperandsSet.count(I_p)){
                loadOperands(I_p, evicRegs, operandOnRegs);
            }
            if(putOnRegsSet.count(I_p)){
                dumpFlag = putOnRegs(I_p, evicRegs, operandOnRegs);
            }
            if(resumeRegsSet.count(I_p)){
                resumeRegs(I_p, evicRegs, dumpFlag);
            }
        }
    }
}

void LessSimpleBackend::depPhi(PHINode *PI){
    Type* phiType = PI->getIncomingValue(0)->getType();
    int phiSize = getAccessSize(phiType);
    BasicBlock &entryBlock = PI->getFunction()->getEntryBlock();
    IRBuilder<> entryBuilder(entryBlock.getFirstNonPHI());
    CallInst *phiPosOnStack_ori = entryBuilder.CreateCall(
        spOffset,
        {ConstantInt::getSigned(IntegerType::getInt64Ty(PI->getContext()), -1)},
        tempPrefix+"pos_"+PI->getName()
    );
    Instruction *phiPosOnStack = phiPosOnStack_ori;
    if(!(phiType->isIntegerTy() &&
        phiType->getIntegerBitWidth()==8
        )){
        Value *posOnStack_cast = entryBuilder.CreateBitCast(
            phiPosOnStack, phiType->getPointerTo(), 
            phiPosOnStack->getName()+"_cast"
        );
        phiPosOnStack = dyn_cast<Instruction>(posOnStack_cast);
        assert(phiPosOnStack);
    }
    int phiPosOffset = frame->putOnStack(phiPosOnStack, false, phiSize);
    phiPosOnStack_ori->setArgOperand(0, 
        ConstantInt::getSigned(IntegerType::getInt64Ty(PI->getContext()), phiPosOffset)
    );
    for(int i = 0; i < PI->getNumIncomingValues(); i++){
        Value* inValue = PI->getIncomingValue(i);
        BasicBlock* inBlock = PI->getIncomingBlock(i);
        IRBuilder<> blockBuilder(inBlock->getTerminator());
        Instruction *storePI = blockBuilder.CreateStore(
            inValue,
            phiPosOnStack
        );
        // loadOperandsSet.insert(storePI);
        // resumeRegsSet.insert(storePI);
    }
    IRBuilder<> PIBuilder(PI);
    Instruction* newPI = PIBuilder.CreateLoad(
        phiPosOnStack,
        PI->getName()
    );
    // loadOperandsSet.insert(newPI);
    // putOnRegsSet.insert(newPI);
    // resumeRegsSet.insert(newPI);
    PI->replaceAllUsesWith(newPI);
    PI->removeFromParent();
}

void LessSimpleBackend::depPhi(Function &F){
    vector<PHINode*> phiList;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            if(PHINode *PI = dyn_cast<PHINode>(&I)){
                phiList.push_back(PI);
            }
        }
    }
    for(PHINode *PI : phiList){
        depPhi(PI);
    }
}

void LessSimpleBackend::depromoteReg(Function &F){
    for(int i = 0; i < F.arg_size(); i++){
        renameArg(*F.getArg(i), i);
    }
    regs = new LessSimpleBackend::Registers(&F, this);
    frame = new LessSimpleBackend::StackFrame(&F, this);
    depPhi(F);
    vector<Instruction*> instList;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            if(!I.isTerminator() && !I.getName().startswith(tempPrefix)){
                loadOperandsSet.insert(&I);
                if(I.hasName()){
                    putOnRegsSet.insert(&I);
                }
                resumeRegsSet.insert(&I);
            }
        }
    }
    regAlloc(F);
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

    string rstHName = getUniqueFnName("__resetHeap", M);
    string rstSName = getUniqueFnName("__resetStack", M);
    string spOffsetName = getUniqueFnName("__spOffset", M);

    Type *VoidTy = Type::getVoidTy(M.getContext());
    PointerType *I8PtrTy = PointerType::getInt8PtrTy(M.getContext());
    IntegerType *I64Ty = IntegerType::getInt64Ty(M.getContext());

    FunctionType *rstTy = FunctionType::get(VoidTy, {VoidTy}, false);
    FunctionType *spOffsetTy = FunctionType::get(I8PtrTy, {I64Ty}, false);

    tempPrefix = getUniquePrefix("temp_p_", M);

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