#include "llvm/Analysis/TargetFolder.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_os_ostream.h"
#include <string>
#include <sstream>
#include <memory>

#include "LessSimpleBackend.h"

using namespace llvm;
using namespace std;

static Value *unCast(CastInst *CI){
    if(SExtInst *SEXTInst = dyn_cast<SExtInst>(CI)){
        return CI;
    }
    Value *operandV = CI->getOperand(0);
    if(CastInst *operandCI = dyn_cast<CastInst>(operandV)){
        return unCast(operandCI);
    }else{
        return operandV;
    }
}

static int nonOffset(Instruction *I){
    for(User *user : I->users()){
        Value *userV = user;
        if(CastInst *CI = dyn_cast<CastInst>(userV)){
            userV = unCast(CI);
        }
        if(Instruction *userInst = dyn_cast<Instruction>(user)){
            if(
                (dyn_cast<StoreInst>(userInst) &&
                userInst->getOperand(1) == I) ||
                (dyn_cast<LoadInst>(userInst) &&
                userInst->getOperand(0) == I)
                ){
                continue;
            }else{
                return 2;
            }
        }
    }
    return 0;
}

static CastInst *getCastInst(Value *from, Type *to, Instruction *insertBefore){
    if(from->getType()->isPointerTy() && to->isPointerTy()){
        return new BitCastInst(from, to, "", insertBefore);
    }else if(from->getType()->isIntegerTy() && to->isPointerTy()){
        return new IntToPtrInst(from, to, "", insertBefore);
    }else if(from->getType()->isPointerTy() && to->isIntegerTy()){
        return new PtrToIntInst(from, to, "", insertBefore);
    }else if(from->getType()->isIntegerTy() && to->isIntegerTy()){
        if(from->getType()->getIntegerBitWidth() > to->getIntegerBitWidth()){
            return new TruncInst(from, to, "", insertBefore);
        }else if(from->getType()->getIntegerBitWidth() < to->getIntegerBitWidth()){
            return new ZExtInst(from, to, "", insertBefore);
        }else{
            assert(false && "cast on same size int");
        }
    }else{
        outs()<<*from<<"\n";
        outs()<<*to<<"\n";
        outs()<<*insertBefore<<"\n";
        assert(false && "unsupported cast");
    }
    return nullptr;
}
static bool usedAfter(Instruction *I, Instruction *I_current, string tempPrefix);
static bool containsUseOf(Instruction *user, Instruction *usee){
    for(int i = 0; i < user->getNumOperands(); i++){
        Value *opV = user->getOperand(i);
        // if(opV == usee){return true;}
        // if(CastInst *CI = dyn_cast<CastInst>(opV)){
        //     opV = unCast(CI);
        // }
        if(opV == usee){return true;}
    }
    return false;
}
static bool usedAfterDFS(Instruction *I, Instruction *I_current, BasicBlock *BB, set<BasicBlock*> &visitedBlockSet){
    if(visitedBlockSet.count(BB)){return false;}
    for(Instruction &instOfBB : BB->getInstList()){
        if(containsUseOf(&instOfBB, I)){
            return true;
        }
    }
    visitedBlockSet.insert(BB);
    for(int i = 0; i < BB->getTerminator()->getNumSuccessors(); i++){
        if(usedAfterDFS(I, I_current, BB->getTerminator()->getSuccessor(i), visitedBlockSet)){return true;}
    }
    return false;
}
static bool _usedAfter(Instruction *I, Instruction *I_current, string tempPrefix){
    Instruction *localTerm = I_current->getParent()->getTerminator();
    Instruction *walkerInst = I_current;
    while(walkerInst != localTerm){
        if(containsUseOf(walkerInst, I)){return true;}
        walkerInst = walkerInst->getNextNode();
    }
    set<BasicBlock*> visitedBlockSet;
    for(int i = 0; i < localTerm->getNumSuccessors(); i++){
        if(usedAfterDFS(I, I_current, localTerm->getSuccessor(i), visitedBlockSet)){return true;}
    }
    return false;
}
static bool usedAfter(Instruction *I, Instruction *I_current, string tempPrefix){
    vector<Instruction*> searchList;
    searchList.push_back(I);
    for(User *user : I->users()){
        if(CastInst *CI = dyn_cast<CastInst>(user)){
            if(CI->getName().startswith(tempPrefix)){
                searchList.push_back(CI);
            }
        }
    }
    for(Instruction *searchI : searchList){
        if(_usedAfter(searchI, I_current, tempPrefix)){
            return true;
        }
    }
    return false;
}

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

static unsigned int align(unsigned int num, int align=8){
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
            if(!usedAfter(frame[i].first, I_current, Backend->getTempPrefix())){
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
    void printFrame(){
        int offset = 0;
        for(auto iter : frame){
            string eleName = "nullptr";
            if(iter.first!=nullptr){
                eleName = iter.first->getName();
            }
            offset += iter.second;
        }
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
    vector<bool> syncFlags;
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
        syncFlags[regPos-1] = false;
    }
    void tryDumpRedundant(Instruction *I_current){
        for(int i = 0; i < regs.size(); i++){
            if(regs[i]==nullptr){
                continue;
            }
            if(!usedAfter(regs[i], I_current, Backend->getTempPrefix())){
                regs[i] = nullptr;
                syncFlags[i] = true;
            }
        }
    }
    int findVictimExcept(Instruction *I_current, vector<int> exceptPosList){
        set<int>possibleRegNumSet;
        for(int i = 1; i <= regs.size(); i++){
            possibleRegNumSet.insert(i);
        }
        for(int i = 0; i < exceptPosList.size(); i++){
            possibleRegNumSet.erase(exceptPosList[i]);
        }
        tryDumpRedundant(I_current);
        for(int possibleRegNum : possibleRegNumSet){
            if(regs[possibleRegNum-1] == nullptr){
                return possibleRegNum;
            }
        }
        // for(int possibleRegNum : possibleRegNumSet){
        //     if(!usedAfter(regs[possibleRegNum-1], I_current, Backend->getTempPrefix())){
        //         return possibleRegNum;
        //     }
        // }
        for(int possibleRegNum : possibleRegNumSet){
            if(syncFlags[possibleRegNum-1]){
                return possibleRegNum;
            }
        }
        for(int possibleRegNum : possibleRegNumSet){
            return -possibleRegNum;
        }
        assert(false);
    }
    Instruction *loadToReg(Instruction *IOnStack, StackFrame *frame,
        Instruction *InsertBefore, int regNum){
        IRBuilder<> Builder(InsertBefore);
        Value *loadOperand = Backend->stackMap[IOnStack];
        Instruction *newInst = Builder.CreateLoad(
            loadOperand,
            genRegName(IOnStack, regNum)
        );
        regs[regNum-1] = IOnStack;
        syncFlags[regNum-1] = true;
        return newInst;
    }
    void storeToFrame(Instruction *IOnReg, StackFrame *frame,
        Instruction *InsertBefore, int regNum){
        if(regNum <= 0){
            regNum = findOnRegs(IOnReg);
            assert(regNum>0);
        }
        if(syncFlags[regNum-1]){
            return;
        }
        IRBuilder<> Builder(InsertBefore);
        if(Backend->stackMap.count(IOnReg)==0){
            AllocaInst *loadOperand = Builder.CreateAlloca(
                IOnReg->getType(),
                nullptr,
                Backend->tempPrefix+"_ltf_"+IOnReg->getName()
            );
             Backend->stackMap[IOnReg] = loadOperand;
        }
        Builder.CreateStore(
            IOnReg,
            Backend->stackMap[IOnReg]
        );
        syncFlags[regNum-1] = true;
        regs[regNum-1] = nullptr;
    }
    string genRegName(Instruction *I, int regNum){
        return "r"+to_string(regNum)+"_"+I->getName().str();
    }
    Registers(Function *F, LessSimpleBackend *Backend):
        F(F),Backend(Backend),regs(vector<Instruction*>(REG_SIZE))
        ,syncFlags(vector<bool>(REG_SIZE)){
        for(int i = 0; i < regs.size(); i++){
            regs[i] = nullptr;
            syncFlags[i] = true;
        }
    }
    Registers(tuple<Function*, LessSimpleBackend*, vector<Instruction*>, vector<bool>> save):
    F(get<0>(save)),Backend(get<1>(save)),regs(get<2>(save)),syncFlags(get<3>(save)){} 
    tuple<Function*, LessSimpleBackend*, vector<Instruction*>, vector<bool>> getSave(){
        return make_tuple<>(
            F, Backend, regs, syncFlags
        );
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
            string regName;
            if(regs[i]==nullptr){
                regName = "nullptr";
            }else if(!regs[i]->hasName()){
                outs()<<"r"<<i+1<<": "<<regs[i]<<"\n";
                continue;
            }else{
                regName = regs[i]->getName();
            }
            outs()<<"r"<<i+1<<": "<<regName<<"\n";
        }
    }
    vector<Instruction*> getRegs(){
        return regs;
    }
    vector<bool> getSyncFlags(){
        return syncFlags;
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
        if(I->getParent()->getTerminator() == I){
            return;
        }
        vector<Instruction*> toMoveToRegs;
        vector<Instruction*> relatedInstList;
        map<Instruction*, Value*>relatedInstMap;
        for(int i = 0; i < I->getNumOperands(); i++){
            Value* operand = I->getOperand(i);
            if(CastInst *CI = dyn_cast<CastInst>(operand)){
                if(CI->getName().startswith(tempPrefix)){
                    operand = unCast(CI);
                }
            }
            if(Instruction* operand_I = dyn_cast<Instruction>(operand)){
                if(!operand_I->getName().startswith(tempPrefix)){
                    relatedInstList.push_back(operand_I);
                    relatedInstMap[operand_I] = I->getOperand(i);
                }
            }
        }
        for(Instruction *relatedInst : relatedInstList){
            int regNum = regs->findOnRegs(relatedInst);
            if(regNum > 0){
                operandOnRegs.push_back(regNum);
                continue;
            }
            if(stackMap.count(relatedInst)){
                toMoveToRegs.push_back(relatedInst);
                continue;
            }
            outs()<<*I<<"\n";
            outs()<<*relatedInst<<"\n";
            outs()<<*I->getFunction();
            assert(false);
        }
        for(Instruction* IOnStack : toMoveToRegs){
            int victimRegNum = regs->findVictimExcept(I, operandOnRegs);
            if(victimRegNum <= 0){
                victimRegNum = -victimRegNum;
                evicRegs.push_back(pair(regs->getInst(victimRegNum), victimRegNum));
                regs->storeToFrame(regs->getInst(victimRegNum), frame, I, victimRegNum);
            }else{
                regs->setInst(IOnStack, victimRegNum);
            }
            Instruction *newInst = regs->loadToReg(IOnStack, frame, I, victimRegNum);
            Value *oldOperand = relatedInstMap[IOnStack];
            if(newInst->getType() != oldOperand->getType()){
                newInst = getCastInst(newInst, oldOperand->getType(), I);
                newInst->setName(tempPrefix+oldOperand->getName()+"_cast");
            }
            I->replaceUsesOfWith(oldOperand, newInst);
            operandOnRegs.push_back(victimRegNum);
        }
}

static int isUsedByItsTerminator(Instruction *I){
    Instruction *term = I->getParent()->getTerminator();
    for(int i = 0; i < term->getNumOperands(); i++){
        if(I == term->getOperand(i)){
            int userNum = 0;
            for(auto user : I->users()){
                userNum++;
            }
            return userNum;
        }
    }
    return 0;
}

bool LessSimpleBackend::putOnRegs(
    Instruction *I, vector<pair<Instruction*, int>> &evicRegs,
    vector<int> &operandOnRegs){
    if((!I->hasName()) || I->getName().startswith(tempPrefix)){
        return false;
    }
    int victimRegNum;
    bool dumpFlag = false;
    if(int useNum = isUsedByItsTerminator(I)){
        victimRegNum = BR_REG;
        if(useNum > 1){
            IRBuilder<> Builder(I->getNextNode());
            Instruction *allocInst = Builder.CreateAlloca(
                I->getType(),
                nullptr,
                tempPrefix+I->getName()+"_pos"
            );
            Instruction *storeInst = Builder.CreateStore(
                I,
                allocInst
            );
            putOnRegsSet.insert(allocInst);
            for(User *user : I->users()){
                Instruction *userI = dyn_cast<Instruction>(userI);
                if(I->getParent()->getTerminator() != userI){
                    IRBuilder<> userInstBuilder(userI);
                    Instruction *loadInst = userInstBuilder.CreateLoad(
                        allocInst,
                        I->getName()
                    );
                    userI->replaceUsesOfWith(I, loadInst);
                    putOnRegsSet.insert(loadInst);
                    resumeRegsSet.insert(loadInst);
                }
            }
        }
    }else{
        victimRegNum = regs->findVictimExcept(I, operandOnRegs);
        if(victimRegNum < 0){
            victimRegNum = -victimRegNum;
            evicRegs.push_back(pair(regs->getInst(victimRegNum), victimRegNum));
            regs->storeToFrame(regs->getInst(victimRegNum), frame, I, victimRegNum);
            dumpFlag = true;
        }else{
            regs->setInst(I, victimRegNum);
        }
    }
    I->setName(regs->genRegName(I, victimRegNum));
    if(victimRegNum != BR_REG){
        regs->setInst(I, victimRegNum);
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
    }
}

void LessSimpleBackend::depAlloca(AllocaInst *AI){
    int offset = frame->putOnStack(AI, true, getAccessSize(AI->getType()->getPointerElementType()));
    IRBuilder<> Builder(AI);
    Instruction *posOnStack = Builder.CreateCall(
        spOffset,
        {ConstantInt::getSigned(IntegerType::getInt64Ty(AI->getContext()), offset)},
        AI->getName()
    );
    if(posOnStack->getType()!=AI->getType()){
        posOnStack = dyn_cast<Instruction>(Builder.CreateBitCast(
            posOnStack,
            AI->getType(),
            tempPrefix+AI->getName()+"_cast"
        ));
    }
    AI->replaceAllUsesWith(posOnStack);
    frame->replaceWith(AI, posOnStack);
    removeInst(AI);
}

void LessSimpleBackend::depAlloca(Function &F){
    vector<AllocaInst*> allocaToDepList;
    for(BasicBlock &B : F){
        for(Instruction &I : B){
            if(AllocaInst *AI = dyn_cast<AllocaInst>(&I)){
                allocaToDepList.push_back(AI);
            }
        }
    }
    for(AllocaInst *allocaToDep : allocaToDepList){
        depAlloca(allocaToDep);
    }
}

void LessSimpleBackend::regAlloc(BasicBlock &BB, set<BasicBlock*> &BBvisited){
    if(BBvisited.count(&BB)){return;}
    else{BBvisited.insert(&BB);}
    vector<Instruction*> initRegs = regs->getRegs();
    for(Instruction &I : BB){
        if(I.getName().startswith(tempPrefix)){continue;}
        vector<pair<Instruction*, int>> evicRegs;
        vector<int> operandOnRegs;
        loadOperands(&I, evicRegs, operandOnRegs);
        putOnRegs(&I, evicRegs, operandOnRegs);
    }
    vector<Instruction*> finalRegs = regs->getRegs();
    IRBuilder<> termBuilder(BB.getTerminator());
    for(int i = 0; i < finalRegs.size(); i++){
        if(initRegs[i] != nullptr &&
            initRegs[i] != finalRegs[i] &&
            usedAfter(initRegs[i], BB.getTerminator(), tempPrefix)){
            if(finalRegs[i]!=nullptr &&
                usedAfter(finalRegs[i], BB.getTerminator(), tempPrefix) && 
                stackMap.count(finalRegs[i])==0){
                regs->storeToFrame(finalRegs[i], frame, BB.getTerminator(), i+1);
            }
            if(stackMap.count(initRegs[i]) == 0){
                assert(false && "cannot rebuild reg");
            }
            regs->loadToReg(initRegs[i], frame, BB.getTerminator(), i+1);
        }
    }
    Registers *originalRegs = this->regs;
    auto save = regs->getSave();
    for(int i = 0; i < BB.getTerminator()->getNumSuccessors(); i++){
        this->regs = new Registers(save);
        regAlloc(*BB.getTerminator()->getSuccessor(i), BBvisited);
        delete(this->regs);
        this->regs = originalRegs;
    }
}

void LessSimpleBackend::_regAlloc(Function &F){
    BasicBlock &BBE = F.getEntryBlock();
    set<BasicBlock*> BBvisited;
    regAlloc(BBE, BBvisited);
}

void LessSimpleBackend::regAlloc(Function& F){
    vector<Instruction*> instToAllocList;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            Instruction *instToAlloc = &I;
            vector<pair<Instruction*, int>> evicRegs;
            vector<int> operandOnRegs;
            bool dumpFlag = false;
            if(instToAlloc->getName().startswith(tempPrefix)){
                continue;
            }
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
}

void LessSimpleBackend::depCast(CastInst *CI){
    if(SExtInst *SEXTInst = dyn_cast<SExtInst>(CI)){
        return;
    }
    CI->setName(tempPrefix+CI->getName());
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

Instruction *LessSimpleBackend::depPhi(PHINode *PI){
    Type* phiType = PI->getIncomingValue(0)->getType();
    int phiSize = getAccessSize(phiType);
    BasicBlock &entryBlock = PI->getFunction()->getEntryBlock();
    IRBuilder<> entryBuilder(entryBlock.getFirstNonPHI());
    AllocaInst *phiTempValOnStack = entryBuilder.CreateAlloca(
        phiType,
        nullptr,
        tempPrefix+"pos_tmep_"+PI->getName()
    );
    putOnRegsSet.insert(phiTempValOnStack);
    AllocaInst *phiRealValOnStack = entryBuilder.CreateAlloca(
        phiType,
        nullptr,
        tempPrefix+"pos_real_"+PI->getName()
    );
    for(int i = 0; i < PI->getNumIncomingValues(); i++){
        Value* inValue = PI->getIncomingValue(i);
        BasicBlock* inBlock = PI->getIncomingBlock(i);
        IRBuilder<> blockBuilder(inBlock->getTerminator());
        Instruction *storePI = blockBuilder.CreateStore(
            inValue,
            phiTempValOnStack
        );
        loadOperandsSet.insert(storePI);
        resumeRegsSet.insert(storePI);
    }
    IRBuilder<> PIBuilder(PI);
    Instruction* updateCarrierDep = PIBuilder.CreateLoad(
        phiTempValOnStack,
        PI->getName()+"_carrier"
    );
    loadOperandsSet.insert(updateCarrierDep);
    putOnRegsSet.insert(updateCarrierDep);
    resumeRegsSet.insert(updateCarrierDep);
    Instruction* updateCarrierArr = PIBuilder.CreateStore(
        updateCarrierDep,
        phiRealValOnStack
    );
    loadOperandsSet.insert(updateCarrierArr);
    resumeRegsSet.insert(updateCarrierArr);
    return phiRealValOnStack;
}

void LessSimpleBackend::__depPhi(PHINode *PI, Instruction *realValPos){
    Function *F = PI->getParent()->getParent();
    vector<pair<Instruction *, int>> newLoadList;
    vector<pair<Instruction *, int>> undefList;
    for(BasicBlock &BB : F->getBasicBlockList()){
        for(Instruction &I : BB){
            for(int i = 0; i < I.getNumOperands(); i++){
                Value *operandV = I.getOperand(i);
                PHINode *operandPI = dyn_cast<PHINode>(operandV);
                if(operandPI == PI){
                    if(dyn_cast<PHINode>(&I)){
                        undefList.push_back(pair(&I, i));
                    }else{
                        newLoadList.push_back(pair(&I, i));
                    }
                }
            }
        }
    }
    for(auto iter : newLoadList){
        IRBuilder<> Builder(iter.first);
        LoadInst *loadRealVal = Builder.CreateLoad(
            realValPos,
            PI->getName()+"_val"
        );
        iter.first->setOperand(iter.second, loadRealVal);
        loadOperandsSet.insert(loadRealVal);
        putOnRegsSet.insert(loadRealVal);
        resumeRegsSet.insert(loadRealVal);
    }
    for(auto iter : undefList){
        iter.first->setOperand(iter.second, UndefValue::get(PI->getType()));
    }
    removeInst(PI);
}

void LessSimpleBackend::depPhi(Function &F){
    vector<PHINode*> phiList;
    map<PHINode*, Instruction*> phiMap;
    map<PHINode*, set<BasicBlock*>> phiUseBlockMap;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            if(PHINode *PI = dyn_cast<PHINode>(&I)){
                phiList.push_back(PI);
            }
        }
    }
    for(PHINode *PI : phiList){
        phiMap[PI] = depPhi(PI);
    }
    for(auto iter : phiMap){
        PHINode *PI = iter.first;
        Instruction *realValPos = iter.second;
        __depPhi(PI, realValPos);
    }
}

void LessSimpleBackend::depGEP(GetElementPtrInst *GEPI){
    for(int i = 1; i < GEPI->getNumOperands(); i++){
        Value *operandV = GEPI->getOperand(i);
        if(CastInst *CI = dyn_cast<CastInst>(operandV)){
            operandV = unCast(CI);
        }
        if(Constant *C = dyn_cast<Constant>(operandV)){
        }else{
            return;
        }
    }
    Value *ptr = GEPI->getOperand(0);
    if(CastInst *CI = dyn_cast<CastInst>(ptr)){
        ptr = unCast(CI);
    }
    if(dyn_cast<AllocaInst>(ptr) ||
        dyn_cast<Constant>(ptr)){
        GEPI->setName(tempPrefix + GEPI->getName());
    }
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

void LessSimpleBackend::depGV(){
    for(auto const &[key, val] : globalVarMap){
        Value *GV = key;
        unsigned int pos = val;
        vector<Instruction*> userInstList;
        for(User *user : GV->users()){
            if(Instruction* userI = dyn_cast<Instruction>(user)){
                userInstList.push_back(userI);
            }
        }
        for(Instruction *userInst : userInstList){
            IntToPtrInst *ITPI = new IntToPtrInst(
                ConstantInt::getSigned(IntegerType::getInt64Ty(userInst->getContext()), pos),
                GV->getType(),
                tempPrefix+"depGV_"+GV->getName(),
                userInst
            );
            userInst->replaceUsesOfWith(GV, ITPI);
        }
    }
}

int LessSimpleBackend::getAccessPos(Value *V){
    if(StoreInst *SI = dyn_cast<StoreInst>(V)){
        return getAccessPos(SI->getOperand(1));
    }else if(LoadInst *LI = dyn_cast<LoadInst>(V)){
        return getAccessPos(LI->getOperand(0));
    }else if(CastInst *CastI = dyn_cast<CastInst>(V)){
        return getAccessPos(unCast(CastI));
    }else if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(V)){
        return getAccessPos(GEPI->getOperand(0));
    }else if(ConstantInt *CInt = dyn_cast<ConstantInt>(V)){
        int64_t posAddr = CInt->getSExtValue();
        if(posAddr >= 20480){
            return POS_HEAP;
        }else if(posAddr <= 10240){
            return POS_STACK;
        }
    }else if(CallInst *CI = dyn_cast<CallInst>(V)){
        Function *calledFunc = CI->getCalledFunction();
        if(calledFunc == spOffset){
            return POS_STACK;
        }else if(calledFunc == malloc){
            return POS_HEAP;
        }
    }
    return POS_UNKNOWN;
}

int LessSimpleBackend::insertRst(BasicBlock &BB){
    int accessPos = POS_UNINIT;
    for(Instruction &I : BB){
        Instruction *I_p = &I;
        int currentAccess = POS_UNINIT;
        if(dyn_cast<StoreInst>(I_p) ||
            dyn_cast<LoadInst>(I_p)){
            currentAccess = getAccessPos(I_p);
        }else{
            continue;
        }
        if(accessPos!=POS_UNINIT && accessPos!=POS_UNKNOWN && currentAccess!=accessPos){
            IRBuilder<> Builder(I_p);
            if(currentAccess == POS_STACK){
                Builder.CreateCall(rstS, {});
            }else if(currentAccess == POS_HEAP){
                Builder.CreateCall(rstH, {});
            }
        }
        accessPos = currentAccess;
    }
    return accessPos;
}

static int getPrevAccessPos(BasicBlock *BB, map<BasicBlock*, int> accessPosMap){
    int accessPos = POS_UNINIT;
    for(BasicBlock *predBlock : predecessors(BB)){
        int prevAccessPos_temp = accessPosMap[predBlock];
        if(prevAccessPos_temp == POS_UNINIT){
            prevAccessPos_temp = getPrevAccessPos(predBlock, accessPosMap);
        }
        if(accessPos == POS_UNINIT){
            accessPos = prevAccessPos_temp;
        }
        if(prevAccessPos_temp == POS_UNKNOWN ||
            prevAccessPos_temp != accessPos){
            return POS_UNKNOWN;
        }
    }
    if(accessPos == POS_UNINIT)
        return POS_UNKNOWN;
    return accessPos;
}

void LessSimpleBackend::insertRst_first(BasicBlock &BB, map<BasicBlock*, int> accessPosMap){
    int prevAccessPos = getPrevAccessPos(&BB, accessPosMap);
    if(prevAccessPos == POS_UNKNOWN){
        return;
    }
    for(Instruction &I : BB){
        int currentAccess = POS_UNINIT;
        if(dyn_cast<StoreInst>(&I) ||
            dyn_cast<LoadInst>(&I)){
            currentAccess = getAccessPos(&I);
        }
        if(currentAccess != POS_UNINIT){
            if(currentAccess != POS_UNKNOWN &&
                currentAccess != prevAccessPos){
                IRBuilder<> Builder(&I);
                if(currentAccess == POS_STACK){
                    Builder.CreateCall(rstS, {});
                }else if(currentAccess == POS_HEAP){
                    Builder.CreateCall(rstH, {});
                }
            }
            return;
        }
    }
}

void LessSimpleBackend::insertRst(Function &F){
    map<BasicBlock*, int> accessPosMap;
    for(BasicBlock &BB : F){
        int lastAccess = insertRst(BB);
        accessPosMap[&BB] = lastAccess;
    }
    for(BasicBlock &BB : F){
        insertRst_first(BB, accessPosMap);
    }
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
            if(!I.getName().startswith(tempPrefix)){
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
    _regAlloc(F); 
    // outs()<<F;
    depAlloca(F);
    insertRst(F);
    placeSpSub(F);
    delete(regs);
    delete(frame);
}

void LessSimpleBackend::buildGVMap(){
    Instruction *firstInst = main->getEntryBlock().getFirstNonPHI();
    IRBuilder<> Builder(firstInst);
    unsigned int addr = 20480;
    for(GlobalValue &GV : main->getParent()->getGlobalList()){
        unsigned int GVSize = -1;
        Type *targetTy = GV.getType()->getPointerElementType();
        if(targetTy->isArrayTy()){
            GVSize = getAccessSize(targetTy);
        }else{
            GVSize = getAccessSize(GV.getType());
        }
        Builder.CreateCall(
            malloc,
            {ConstantInt::getSigned(IntegerType::getInt64Ty(main->getContext()), GVSize)},
            GV.getName()+"_pos"
        );
        globalVarMap[&GV] = addr;
        addr += align(GVSize);
    }
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

    malloc = nullptr;
    main = nullptr;

    for(Function&F : M.getFunctionList()){
        if(F.getName()=="malloc"){
            malloc = &F;
        }else if(F.getName()=="main"){
            main = &F;
        }
    }

    if(malloc==nullptr){
        malloc = Function::Create(spOffsetTy, Function::ExternalLinkage, "malloc", M);
    }
    assert(main!=nullptr);

    buildGVMap();
    depGV();

    for(Function &F : M.getFunctionList()){
        if(!F.isDeclaration()){
            depromoteReg(F);
        }
    }

    outs() << M;

    // Now, let's emit assembly!
    vector<std::string> dummyFunctionName = {
        rstHName, rstSName, spOffsetName, spSubName, tempPrefix };
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
