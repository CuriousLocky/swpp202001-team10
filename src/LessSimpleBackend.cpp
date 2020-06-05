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

static unsigned int align(unsigned int num, int align=16){
    return ((num/align + !!(num%align))*align);
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
      if (!BB.hasName()){
          BB.setName(&F.getEntryBlock() == &BB ? ".entry" : ".bb");
      }else if(!BB.getName().startswith(".")){
          BB.setName("."+BB.getName());
      }

      for (Instruction &I : BB){
        if (!I.hasName() && !I.getType()->isVoidTy()){
          I.setName("tmp");                 
        }
      }
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
    int getMaxStackSize(){return maxStackSize;}
    int putOnStack(Instruction* I, bool dumpFlag=true, int size=-1, int alignment=-1){
        return putOnStack(I, I, dumpFlag, size, alignment);
    }
    int putOnStack(Instruction* I, Instruction *I_current, bool dumpFlag=true, int size=-1, int alignment=-1){
        int offset = findOnStack(I);
        if(offset < 0){
            offset = 0;
        }else{
            return offset;
        }
        if(dumpFlag){
            tryDumpRedundant(I_current);
        }
        if(size <= 0){
            size = getAccessSize(I->getType());
        }
        if(alignment <= 0){
            alignment = size;
        }
        bool onStackFlag = false;
        for(int i = 0; i < frame.size(); i++){
            int comPos = align(offset, alignment);
            auto content = frame[i];
            if(content.first==nullptr){
                int emptyBegin = comPos;
                int emptyEnd = offset + content.second;
                int emptySize = emptyEnd - emptyBegin;
                if(emptySize >= size){
                    frame[i] = pair(I, size);
                    if(emptySize > size){
                        frame.insert(frame.begin()+i+1, pair(nullptr, emptySize-size));
                    }
                    if(emptyBegin > offset){
                        frame.insert(frame.begin()+i-1, pair(nullptr, emptyBegin-offset));
                    }
                    onStackFlag = true;
                    break;
                }
            }
            offset += content.second;
        }
        if(!onStackFlag){
            int comPos = align(offset, alignment);
            auto new_content = pair(I, size);
            if(comPos > offset){
                frame.push_back(pair(nullptr, comPos-offset));
                offset = comPos;
            }
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
    arg.setName("__arg" + to_string(num)+"__");
}

Function *LessSimpleBackend::getSpOffset(){return spOffset;}
Function *LessSimpleBackend::getRstH(){return rstH;}
Function *LessSimpleBackend::getRstS(){return rstS;}
string LessSimpleBackend::getTempPrefix(){return tempPrefix;}

void LessSimpleBackend::removeInst(Instruction *I){
    if(loadOperandsSet.count(I)){
        loadOperandsSet.erase(I);
    }
    if(putOnRegsSet.count(I)){
        putOnRegsSet.erase(I);
    }
    if(resumeRegsSet.count(I)){
        resumeRegsSet.erase(I);
    }
    I->removeFromParent();
    I->deleteValue();
}

void LessSimpleBackend::loadRelatedOperands(
    Instruction *tempI,
    vector<Instruction*> &relatedInstList){
    for(int i = 0; i < tempI->getNumOperands(); i++){
        Value *operandV = tempI->getOperand(i);
        if(Instruction *operandI = dyn_cast<Instruction>(operandV)){
            if(operandI->getName().startswith(tempPrefix)){
                loadRelatedOperands(operandI, relatedInstList);
            }else{
                relatedInstList.push_back(operandI);
            }
        }
    }
}

void LessSimpleBackend::loadOperands(
    Instruction *I, vector<pair<Instruction*, int>> &evicRegs, 
    vector<int> &operandOnRegs){
        vector<Instruction*> toMoveToRegs;
        IRBuilder<> Builder(I);
        for(int i = 0; i < I->getNumOperands(); i++){
            Value* operand = I->getOperand(i);
            if(Instruction* operand_I = dyn_cast<Instruction>(operand)){
                vector<Instruction*> relatedInstList;
                if(operand->getName().startswith(tempPrefix)){
                    continue;
                }else{
                    relatedInstList.push_back(operand_I);
                }
                for(Instruction *relatedInst : relatedInstList){
                    int regNum = regs->findOnRegs(relatedInst);
                    if(regNum > 0){
                        operandOnRegs.push_back(regNum-1);
                        continue;
                    }
                    int stackOffset = frame->findOnStack(relatedInst);
                    if(stackOffset >= 0){
                        toMoveToRegs.push_back(relatedInst);
                        continue;
                    }                    
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
    if(AllocaInst *AI = dyn_cast<AllocaInst>(I)){
        I = depAlloca(AI);
        if(I->getName().startswith(tempPrefix)){
            return false;
        }
    }
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
    if(AllocaInst *AI = dyn_cast<AllocaInst>(I)){
        depAlloca(AI);
    }
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

static bool nonOffset(Instruction *I){
    bool nonOffsetFlag = false;
    for(User *user : I->users()){
        if(Instruction *userInst = dyn_cast<Instruction>(user)){
            if(dyn_cast<CastInst>(userInst)){
                nonOffsetFlag |= nonOffset(userInst);
            }else if(
                    (dyn_cast<StoreInst>(userInst) &&
                    userInst->getOperand(1) == I) ||
                    (dyn_cast<LoadInst>(userInst) &&
                    userInst->getOperand(0) == I)
                ){
                continue;
            }else{
                nonOffsetFlag = true;
            }
        }
    }
    return nonOffsetFlag;
}

Instruction *LessSimpleBackend::depAlloca(AllocaInst *AI){
    unsigned int allocaSize = getAccessSize(AI->getType());
    int offset = frame->putOnStack(AI, true, allocaSize, AI->getAlignment());
    IRBuilder<> Builder(AI);
    Instruction *posOnStack = Builder.CreateCall(
        spOffset,
        {ConstantInt::getSigned(IntegerType::getInt64Ty(AI->getContext()), offset)},
        AI->getName()
    );
    if(!nonOffset(AI)){
        posOnStack->setName(tempPrefix+posOnStack->getName());
    }
    Value *posOnStack_castV = Builder.CreateBitCast(
        posOnStack,
        AI->getType(),
        posOnStack->getName()+"_cast"
    );
    CastInst *posOnStack_cast = dyn_cast<CastInst>(posOnStack_castV);
    AI->replaceAllUsesWith(posOnStack_cast);
    depCast(posOnStack_cast);
    frame->replaceWith(AI, posOnStack);
    removeInst(AI);
    return posOnStack;
}

void LessSimpleBackend::regAlloc(Function& F){
    vector<Instruction*> instToAllocList;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            instToAllocList.push_back(&I);
        }
    }
    for(Instruction *instToAlloc : instToAllocList){
        vector<pair<Instruction*, int>> evicRegs;
        vector<int> operandOnRegs;
        bool dumpFlag = false;
        if(loadOperandsSet.count(instToAlloc)){
            loadOperands(instToAlloc, evicRegs, operandOnRegs);
        }
        if(putOnRegsSet.count(instToAlloc)){
            dumpFlag = putOnRegs(instToAlloc, evicRegs, operandOnRegs);
        }
        if(resumeRegsSet.count(instToAlloc)){
            resumeRegs(instToAlloc, evicRegs, dumpFlag);
        }        
    }
}

void LessSimpleBackend::depCast(CastInst *CI){
    Value *sourceV = CI->getOperand(0);
    Type *dstTy = CI->getDestTy();
    Instruction::CastOps op = CI->getOpcode();
    vector<Instruction*> userInstList;
    for(User *user : CI->users()){
        if(Instruction *userInst = dyn_cast<Instruction>(user)){
            userInstList.push_back(userInst);
        }
    }
    for(Instruction *userInst : userInstList){
        IRBuilder<> Builder(userInst);
        Value *newBCV = Builder.CreateCast(
            op,
            sourceV,
            dstTy,
            tempPrefix+sourceV->getName()+"_cast"
        );
        userInst->replaceUsesOfWith(CI, newBCV);
    }
    removeInst(CI);
}

void LessSimpleBackend::depCast(Function &F){
    vector<CastInst*> BCIList;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            if(CastInst *PI = dyn_cast<CastInst>(&I)){
                BCIList.push_back(PI);
            }
        }
    }
    for(CastInst *BCI : BCIList){
        depCast(BCI);
    }
}

void LessSimpleBackend::depPhi(PHINode *PI){
    Type* phiType = PI->getIncomingValue(0)->getType();
    int phiSize = getAccessSize(phiType);
    BasicBlock &entryBlock = PI->getFunction()->getEntryBlock();
    IRBuilder<> entryBuilder(entryBlock.getFirstNonPHI());
    AllocaInst *phiPosOnStack = entryBuilder.CreateAlloca(
        phiType,
        nullptr,
        "pos_"+PI->getName()
    );
    putOnRegsSet.insert(phiPosOnStack);
    for(int i = 0; i < PI->getNumIncomingValues(); i++){
        Value* inValue = PI->getIncomingValue(i);
        BasicBlock* inBlock = PI->getIncomingBlock(i);
        IRBuilder<> blockBuilder(inBlock->getTerminator());
        Instruction *storePI = blockBuilder.CreateStore(
            inValue,
            phiPosOnStack
        );
        loadOperandsSet.insert(storePI);
        resumeRegsSet.insert(storePI);
    }
    IRBuilder<> PIBuilder(PI);
    Instruction* newPI = PIBuilder.CreateLoad(
        phiPosOnStack,
        PI->getName()
    );
    loadOperandsSet.insert(newPI);
    putOnRegsSet.insert(newPI);
    resumeRegsSet.insert(newPI);
    PI->replaceAllUsesWith(newPI);
    removeInst(PI);
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

void LessSimpleBackend::depGEP(GetElementPtrInst *GEPI){
    Value *offsetV = GEPI->getOperand(1);
    vector<Instruction*> userInstList;
    set<Instruction*> onRegSet;
    for(User *user : GEPI->users()){
        if(Instruction *userInst = dyn_cast<Instruction>(user)){
            userInstList.push_back(userInst);
            if(
                dyn_cast<ConstantInt>(offsetV) &&
                (
                    (dyn_cast<StoreInst>(userInst) &&
                    userInst->getOperand(1) == GEPI) ||
                    (dyn_cast<LoadInst>(userInst) &&
                    userInst->getOperand(0) == GEPI)
                )
                ){
                continue;
            }else{
                onRegSet.insert(userInst);
            }
        }
    }
    for(Instruction *userInst : userInstList){
        IRBuilder<> Builder(userInst);
        Value *newGEPV = Builder.CreateGEP(
            GEPI->getOperand(0),
            offsetV,
            tempPrefix+GEPI->getName()
        );
        userInst->replaceUsesOfWith(GEPI, newGEPV);
        Instruction *newGEPI = dyn_cast<Instruction>(newGEPV);
        loadOperandsSet.insert(newGEPI);
        resumeRegsSet.insert(newGEPI);
        if(onRegSet.count(userInst)){
            newGEPV->setName("ptr_"+GEPI->getName());
            putOnRegsSet.insert(newGEPI);
        }
    }
    removeInst(GEPI);
}

void LessSimpleBackend::depGEP(Function &F){
    vector<GetElementPtrInst*> GEPIList;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(&I)){
                GEPIList.push_back(GEPI);
            }
        }
    }
    for(GetElementPtrInst *GEPI : GEPIList){
        depGEP(GEPI);
    }
}

void LessSimpleBackend::placeSpSub(Function &F){
    BasicBlock &entryBlock = F.getEntryBlock();
    Instruction *entryInst = entryBlock.getFirstNonPHI();
    IRBuilder<> Builder(entryInst);
    int maxStackUsage = frame->getMaxStackSize();
    maxStackUsage = align(maxStackUsage);
    Builder.CreateCall(
        spSub,
        {ConstantInt::getSigned(IntegerType::getInt64Ty(F.getContext()), maxStackUsage)}
    );
}

void LessSimpleBackend::depromoteReg(Function &F){
    for(int i = 1; i <= F.arg_size(); i++){
        renameArg(*F.getArg(i-1), i);
    }
    regs = new LessSimpleBackend::Registers(&F, this);
    frame = new LessSimpleBackend::StackFrame(&F, this);
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
    depCast(F);
    depPhi(F);
    depGEP(F);
    regAlloc(F);
    placeSpSub(F);
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
    // ConstExprToInsts CEI;
    // CEI.visit(M);

    string rstHName = getUniqueFnName("__resetHeap", M);
    string rstSName = getUniqueFnName("__resetStack", M);
    string spOffsetName = getUniqueFnName("__spOffset", M);
    string spSubName = getUniqueFnName("__spSub", M);

    Type *VoidTy = Type::getVoidTy(M.getContext());
    PointerType *I8PtrTy = PointerType::getInt8PtrTy(M.getContext());
    IntegerType *I64Ty = IntegerType::getInt64Ty(M.getContext());

    FunctionType *rstTy = FunctionType::get(VoidTy, {}, false);
    FunctionType *spOffsetTy = FunctionType::get(I8PtrTy, {I64Ty}, false);
    FunctionType *spSubTy = FunctionType::get(VoidTy, {I64Ty}, false);

    tempPrefix = getUniquePrefix("temp_p_", M);

    rstH = Function::Create(rstTy, Function::ExternalLinkage, rstHName, M);
    rstS = Function::Create(rstTy, Function::ExternalLinkage, rstSName, M);
    spOffset = Function::Create(spOffsetTy, Function::ExternalLinkage, spOffsetName, M);
    spSub = Function::Create(spSubTy, Function::ExternalLinkage, spSubName, M);

    for(Function &F : M.getFunctionList()){
        if(!F.isDeclaration()){
            depromoteReg(F);
        }
    }

    outs() << M;

    // Now, let's emit assembly!
    vector<std::string> dummyFunctionName = { 
        rstHName, rstSName, spOffsetName, spSubName };
    error_code EC;
    raw_ostream *os =
        outputFile == "-" ? &outs() : new raw_fd_ostream(outputFile, EC);

    if (EC) {
        errs() << "Cannot open file: " << outputFile << "\n";
        exit(1);
    }

    NewAssemblyEmitter Emitter(os, dummyFunctionName);
    Emitter.run(&M);

    if (os != &outs()) delete os;
    
    return PreservedAnalyses::all();
}