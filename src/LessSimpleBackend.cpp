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
static bool containsUseOf(Instruction *user, Instruction *usee){
    for(int i = 0; i < user->getNumOperands(); i++){
        Value *opV = user->getOperand(i);
        if(opV == usee){return true;}
    }
    return false;
}
static bool usedAfterDFS(
    Instruction *I, BasicBlock *BB,
    set<BasicBlock*> &visitedBlockSet, bool selfFlag){
    if(visitedBlockSet.count(BB)){return 0;}
    int distance = 1;
    for(Instruction &instOfBB : BB->getInstList()){
        if(&instOfBB==I && selfFlag){return 0;}
        if(containsUseOf(&instOfBB, I)){return distance;}
        distance ++;
    }
    vector<int> distanceList;
    visitedBlockSet.insert(BB);
    for(int i = 0; i < BB->getTerminator()->getNumSuccessors(); i++){
        if(int dis = usedAfterDFS(I, BB->getTerminator()->getSuccessor(i), visitedBlockSet, selfFlag)){
            distanceList.push_back(distance+dis);
        }
    }
    if(distanceList.size()==0){return 0;}
    std::sort(distanceList.begin(), distanceList.end());
    return distanceList[0];
}
static int _usedAfter(Instruction *I, Instruction *I_current, string tempPrefix, bool selfFlag=false){
    Instruction *localTerm = I_current->getParent()->getTerminator();
    Instruction *walkerInst = I_current;
    int distance = 1;
    while(walkerInst != localTerm){
        if(walkerInst==I && selfFlag){return 0;}
        if(containsUseOf(walkerInst, I)){return distance;}
        walkerInst = walkerInst->getNextNode();
        distance ++;
    }
    set<BasicBlock*> visitedBlockSet;
    vector<int> distanceList;
    for(int i = 0; i < localTerm->getNumSuccessors(); i++){
        if(int dis = usedAfterDFS(I, localTerm->getSuccessor(i), visitedBlockSet, selfFlag)){
            distanceList.push_back(dis+distance);
        }
    }
    if(distanceList.size()==0){return 0;}
    std::sort(distanceList.begin(), distanceList.end());
    return distanceList[0];
}
static int usedAfter(Instruction *I, Instruction *I_current, string tempPrefix, bool selfFlag=true){
    if (I == nullptr) { return 0; }
    vector<Instruction*> searchList;
    vector<int> distanceList;
    // searchList.push_back(I);
    for(User *user : I->users()){
        if(CastInst *CI = dyn_cast<CastInst>(user)){
            if(CI->getName().startswith(tempPrefix)){
                searchList.push_back(CI);
            }
        }
    }
    if(int dis = _usedAfter(I, I_current, tempPrefix, selfFlag)){
        distanceList.push_back(dis);
    }
    for(Instruction *searchI : searchList){
        if(int dis = _usedAfter(searchI, I_current, tempPrefix)){
            distanceList.push_back(dis);
        }
    }
    if(distanceList.size()==0){return 0;}
    std::sort(distanceList.begin(), distanceList.end());
    return distanceList[0];
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
            if(!usedAfter(frame[i].first, I_current, Backend->getTempPrefix(), false)){
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
                        frame.insert(frame.begin()+i, pair(nullptr, emptyBegin-offset));
                    }
                    onStackFlag = true;
                    offset = emptyBegin;
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
            outs()<<to_string(offset)<<": "<<eleName+" "<<"\n";
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
        assert(regPos>0 && regPos<=REG_SIZE_ALL);
        return regs[regPos-1];
    }
    void setInst(Instruction *I, int regPos){
        assert(regPos>0 && regPos<=REG_SIZE_ALL);
        regs[regPos-1] = I;
        syncFlags[regPos-1] = false;
    }
    void tryDumpRedundant(Instruction *I_current){
        for(int i = 0; i < REG_SIZE; i++){
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
        set<int> possibleRegNumSet;
        for(int i = 1; i <= REG_SIZE; i++){
            possibleRegNumSet.emplace(i);
        }
        for(int i = 0; i < exceptPosList.size(); i++){
            possibleRegNumSet.erase(exceptPosList[i]);
        }

        std::vector<std::tuple<int, bool, int>> v;
        for (auto i : possibleRegNumSet) {
            int d = usedAfter(regs[i-1], I_current, Backend->getTempPrefix());
            if (!d) { return i; }
            bool t = syncFlags[i-1];
            std::tuple<int, bool, int> tup = make_tuple(i, t, d);
            v.push_back(tup);
        }
        assert(!v.empty());

        auto less = [](auto &left, auto &right) {
        // Check if left should be selected before right
        // Rule 1. Select "fresh" first
        // Rule 2. Select larger usedAfter value first
            if (get<1>(left)) {
                if (get<1>(right)) { return get<2>(left) >= get<2>(right); }
                else { return true; }
            }
            else {
                if (get<1>(right)) { return false; }
                else { return get<2>(left) >= get<2>(right); }
            }
        };
        std::sort(v.begin(), v.end(), less);

        return get<1>(v[0]) ? get<0>(v[0]) : -get<0>(v[0]);
        // for(int possibleRegNum : possibleRegNumSet){
        //     if(regs[possibleRegNum-1] == nullptr){
        //         return possibleRegNum;
        //     }
        // }
        // for(int possibleRegNum : possibleRegNumSet){
        //     if(syncFlags[possibleRegNum-1]){
        //         return possibleRegNum;
        //     }
        // }
        // for(int possibleRegNum : possibleRegNumSet){
        //     return -possibleRegNum;
        // }
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
            IRBuilder<> entryBuilder(InsertBefore->getFunction()->getEntryBlock().getFirstNonPHI());
            AllocaInst *loadOperand = entryBuilder.CreateAlloca(
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
        F(F),Backend(Backend),regs(vector<Instruction*>(REG_SIZE_ALL))
        ,syncFlags(vector<bool>(REG_SIZE_ALL)){
        for(int i = 0; i < regs.size(); i++){
            regs[i] = nullptr;
            syncFlags[i] = true;
        }
    }
    Registers(tuple<Function*, LessSimpleBackend*, vector<Instruction*>, vector<bool>> save):
        F(get<0>(save)),Backend(get<1>(save)),regs(get<2>(save)),syncFlags(get<3>(save)){}
    tuple<Function*, LessSimpleBackend*, vector<Instruction*>, vector<bool>> getSave(){
        return make_tuple<>(F, Backend, regs, syncFlags);
    }
    int findOnRegs(Instruction* I){
        for(int i = 0; i < REG_SIZE_ALL; i++){
            if(regs[i] == I){return i+1;}
        }
        return -1;
    }
    void printRegs(){
        for(int i = 0; i < REG_SIZE_ALL; i++){
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
    I->removeFromParent();
    I->deleteValue();
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
                if(!relatedInst->getName().startswith("r"+to_string(regNum)+"_")){
                    IRBuilder<> Builder(I);
                    Instruction *regSwArg = relatedInst;
                    if(relatedInst->getType()!=Type::getInt64Ty(I->getContext())){
                        regSwArg = getCastInst(relatedInst, Type::getInt64Ty(I->getContext()), I);
                        regSwArg->setName(tempPrefix+relatedInst->getName()+"_cast");
                    }
                    CallInst *relocatedReg = Builder.CreateCall(
                        regSwitch, {regSwArg},
                        regs->genRegName(relatedInst, regNum)
                    );
                    Instruction *newOperand = relocatedReg;
                    if(relocatedReg->getType()!=relatedInstMap[relatedInst]->getType()){
                        newOperand = getCastInst(relocatedReg, relatedInstMap[relatedInst]->getType(), I);
                        newOperand->setName(tempPrefix+relocatedReg->getName()+"_cast");
                    }
                    I->replaceUsesOfWith(relatedInstMap[relatedInst], newOperand);
                }
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

static bool isUsedByItsTerminator(Instruction *I){
    Instruction *term = I->getParent()->getTerminator();
    for(int i = 0; i < term->getNumOperands(); i++){
        Value *operandV = term->getOperand(i);
        if(auto*CI = dyn_cast<CastInst>(operandV)){operandV=unCast(CI);}
        if(I == term->getOperand(i)){return true;}
    }
    return false;
}

bool LessSimpleBackend::putOnRegs(
    Instruction *I, vector<pair<Instruction*, int>> &evicRegs,
    vector<int> &operandOnRegs){
    if((!I->hasName()) || I->getName().startswith(tempPrefix)){
        return false;
    }
    int victimRegNum;
    bool dumpFlag = false;
    if(isUsedByItsTerminator(I)){
        victimRegNum = BR_REG;
    }else{
        victimRegNum = regs->findVictimExcept(I, operandOnRegs);
        if(victimRegNum < 0){
            victimRegNum = -victimRegNum;
            evicRegs.push_back(pair(regs->getInst(victimRegNum), victimRegNum));
            regs->storeToFrame(regs->getInst(victimRegNum), frame, I, victimRegNum);
            dumpFlag = true;
        }
    }
    regs->setInst(I, victimRegNum);
    I->setName(regs->genRegName(I, victimRegNum));
    if(victimRegNum != BR_REG){
        regs->setInst(I, victimRegNum);
    }else if(I->isUsedOutsideOfBlock(I->getParent())){
        regs->storeToFrame(I, frame, I->getNextNode(), BR_REG);
    }
    return dumpFlag;
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
    for(int i = 0; i < REG_SIZE; i++){
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

void LessSimpleBackend::regAlloc(Function &F){
    BasicBlock &BBE = F.getEntryBlock();
    set<BasicBlock*> BBvisited;
    regAlloc(BBE, BBvisited);
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

set<Instruction*> LessSimpleBackend::depPhi(Function &F){
    vector<PHINode*> phiList;
    map<PHINode*, Instruction*> phiMap;
    map<PHINode*, set<BasicBlock*>> phiUseBlockMap;
    set<Instruction*> newPhiSet;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            if(PHINode *PI = dyn_cast<PHINode>(&I)){
                phiList.push_back(PI);
            }
        }
    }
    for(PHINode *PI : phiList){
        IRBuilder<> entryBuilder(F.getEntryBlock().getFirstNonPHI());
        IRBuilder<> Builder(PI);
        AllocaInst *phiPos = entryBuilder.CreateAlloca(
            PI->getType(),
            nullptr,
            tempPrefix+"pos_"+PI->getName()
        );
        LoadInst *newPI = Builder.CreateLoad(
            phiPos,
            PI->getName()
        );
        for(int i = 0; i < PI->getNumIncomingValues(); i++){
            Value* inValue = PI->getIncomingValue(i);
            BasicBlock* inBlock = PI->getIncomingBlock(i);
            IRBuilder<> blockBuilder(inBlock->getTerminator());
            Instruction *storePI = blockBuilder.CreateStore(
                inValue,
                phiPos
            );
        }
        PI->replaceAllUsesWith(newPI);
        removeInst(PI);
        newPhiSet.insert(newPI);
    }
    return newPhiSet;
}

void LessSimpleBackend::phiUpdatePatch(set<Instruction*> &newPhiSet){
    for(Instruction* newPhi : newPhiSet){
        if(stackMap.count(newPhi)==0){continue;}
        StoreInst *phiUpdateInst = new StoreInst(
            newPhi,
            stackMap[newPhi],
            newPhi->getNextNode()
        );
        // outs()<<*newPhi->getParent();
    }
}

void LessSimpleBackend::depGEP(GetElementPtrInst *GEPI){
    Value *ptr = GEPI->getOperand(0);
    if(CastInst *CI = dyn_cast<CastInst>(ptr)){
        ptr = unCast(CI);}
    if(dyn_cast<AllocaInst>(ptr) ||
        dyn_cast<Constant>(ptr)){
        if(ptr->hasName() && !(ptr->getName().startswith(tempPrefix))){return;}
        for(int i = 1; i < GEPI->getNumOperands(); i++){
            Value *operandV = GEPI->getOperand(i);
            if(CastInst *CI = dyn_cast<CastInst>(operandV)){
                operandV = unCast(CI);}
            if(Constant *C = dyn_cast<Constant>(operandV)){
            }else{return; }
        }
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
        }else if(calledFunc == regSwitch){
            return getAccessPos(CI->getArgOperand(0));
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
        }else{continue;}
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
    if(accessPos == POS_UNINIT){return POS_UNKNOWN;}
    return accessPos;
}

void LessSimpleBackend::insertRst_first(BasicBlock &BB, map<BasicBlock*, int> accessPosMap){
    int prevAccessPos = getPrevAccessPos(&BB, accessPosMap);
    if(prevAccessPos == POS_UNKNOWN){return;}
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

static Instruction* delayAllocaDFS(AllocaInst *AI, set<BasicBlock*> visitedBB, BasicBlock *BB = nullptr){
    // BasicBlock *BB = AI->getParent();
    if(BB==nullptr){BB=AI->getParent();}
    if(visitedBB.count(BB)){return nullptr;}
    for(Instruction &I : BB->getInstList()){
        if(containsUseOf(&I, AI)){return &I;}
    }
    visitedBB.insert(BB);
    Instruction *result = nullptr;
    for(int i = 0; i < BB->getTerminator()->getNumSuccessors(); i++){
        Instruction *childResult = delayAllocaDFS(AI, visitedBB, BB->getTerminator()->getSuccessor(i));
        if(childResult!=nullptr){
            if(result == nullptr){result = childResult;}
            else{return BB->getTerminator();}
        }
    }
    return result;
}

static void _delayAlloca(AllocaInst *AI){
    set<BasicBlock*> visitedBB;
    Instruction *latestSafePos = delayAllocaDFS(AI, visitedBB);
    IRBuilder<> Builder(latestSafePos);
    AllocaInst *newAlloca = Builder.CreateAlloca(
        AI->getAllocatedType(),
        AI->getArraySize(),
        AI->getName()
    );
    AI->replaceAllUsesWith(newAlloca);
    AI->removeFromParent();
    AI->deleteValue();
}

void LessSimpleBackend::delayAlloca(Function &F){
    vector<AllocaInst*> allocaInstList;
    for(BasicBlock &BB : F){
        for(Instruction &I : BB){
            if(AllocaInst *AI = dyn_cast<AllocaInst>(&I)){
                if(AI->getName().startswith(tempPrefix)){
                    allocaInstList.push_back(AI);
                }
            }
        }
    }
    for(AllocaInst *AI : allocaInstList){
        _delayAlloca(AI);
    }
}

void LessSimpleBackend::depromoteReg(Function &F){
    for(int i = 1; i <= F.arg_size(); i++){
        renameArg(*F.getArg(i-1), i);
    }
    regs = new LessSimpleBackend::Registers(&F, this);
    frame = new LessSimpleBackend::StackFrame(&F, this);
    depCast(F);
    auto newPhiSet = depPhi(F);
    depGEP(F);
    regAlloc(F);
    delayAlloca(F);
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
    if (verifyModule(M, &errs(), nullptr)){exit(1);}

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
    string regSwitchName = getUniqueFnName("__regSwtich", M);

    Type *VoidTy = Type::getVoidTy(M.getContext());
    PointerType *I8PtrTy = PointerType::getInt8PtrTy(M.getContext());
    IntegerType *I64Ty = IntegerType::getInt64Ty(M.getContext());

    FunctionType *rstTy = FunctionType::get(VoidTy, {}, false);
    FunctionType *spOffsetTy = FunctionType::get(I8PtrTy, {I64Ty}, false);
    FunctionType *spSubTy = FunctionType::get(VoidTy, {I64Ty}, false);
    FunctionType *regSwitchTy = FunctionType::get(I64Ty, {I64Ty}, false);

    tempPrefix = getUniquePrefix("temp_p_", M);

    rstH = Function::Create(rstTy, Function::ExternalLinkage, rstHName, M);
    rstS = Function::Create(rstTy, Function::ExternalLinkage, rstSName, M);
    spOffset = Function::Create(spOffsetTy, Function::ExternalLinkage, spOffsetName, M);
    spSub = Function::Create(spSubTy, Function::ExternalLinkage, spSubName, M);
    regSwitch = Function::Create(regSwitchTy, Function::ExternalLinkage, regSwitchName, M);

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

    if(printDepromotedModule){
        outs() << M;
    }


    // Now, let's emit assembly!
    vector<std::string> dummyFunctionName = {
        rstHName, rstSName, spOffsetName, spSubName, regSwitchName, tempPrefix };
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
