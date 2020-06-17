#ifndef LESS_SIMPLE_BACKEND_H
#define LESS_SIMPLE_BACKEND_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <map>
#include <set>

#define REG_SIZE 15
#define BR_REG (REG_SIZE+1)
#define REG_SIZE_ALL (REG_SIZE+1)

#define POS_STACK 1
#define POS_UNKNOWN 0
#define POS_HEAP -1
#define POS_UNINIT -2

class LessSimpleBackend : public llvm::PassInfoMixin<LessSimpleBackend> {
  std::string outputFile;
  std::string tempPrefix;
  bool printDepromotedModule;
  std::map<llvm::Instruction*, llvm::Value*> stackMap;
  llvm::Function *spOffset;
  llvm::Function *spSub;
  llvm::Function *rstH;
  llvm::Function *rstS;
  llvm::Function *malloc;
  llvm::Function *main;
  llvm::Function *regSwitch;
  std::map<llvm::Value*, unsigned int> globalVarMap;
  unsigned globalVarOnStackSize;
  std::map<llvm::Value*, unsigned int> globalVarOnStackMap;
  class Registers;
  class StackFrame;
  Registers *regs;
  StackFrame *frame;
  void depromoteReg(llvm::Function &F);
  int getAccessPos(llvm::Value *V);
  llvm::Function* getSpOffsetFn();
  void removeInst(llvm::Instruction *I);
  void loadOperands(
    llvm::Instruction *I,
    std::vector<std::pair<llvm::Instruction*, int>> &evicRegs,
    std::vector<int> &operandOnRegs);
  bool putOnRegs(
    llvm::Instruction *I,
    std::vector<std::pair<llvm::Instruction*, int>> &evicRegs,
    std::vector<int> &operandOnRegs);
  void resumeRegs(
    llvm::Instruction *I,
    std::vector<std::pair<llvm::Instruction*, int>> &evicRegs,
    bool dumpFlag);
  void depAlloca(llvm::AllocaInst *AI);
  void depAlloca(llvm::Function &F);
  void depCast(llvm::CastInst *BCI);
  void depCast(llvm::Function &F);
  std::set<llvm::Instruction*> depPhi(llvm::Function &F);
  void phiUpdatePatch(std::set<llvm::Instruction*> &newPhiSet);
  void depGEP(llvm::GetElementPtrInst *GEPI);
  void depGEP(llvm::Function &F);
  void depGV();
  void delayAlloca(llvm::Function &F);
  int insertRst(llvm::BasicBlock &BB);
  void insertRst_first(
    llvm::BasicBlock &BB,
    std::map<llvm::BasicBlock*, int> accessPosMap);
  void insertRst(llvm::Function &F);
  void regAlloc(llvm::BasicBlock &BB, std::set<llvm::BasicBlock*> &BBvisited);
  void regAlloc(llvm::Function &F);
  void placeSpSub(llvm::Function &F);
  void buildGVMap();
  void moveGVToStack();
public:
  LessSimpleBackend(std::string outputFile, bool printDepromotedModule) :
      outputFile(outputFile), printDepromotedModule(printDepromotedModule) {}
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
  llvm::Function *getSpOffset();
  llvm::Function *getRstH();
  llvm::Function *getRstS();
  std::string getTempPrefix();
};

unsigned getAccessSize(llvm::Type *T);

class NewAssemblyEmitter {
  llvm::raw_ostream *fout;
  std::vector<std::string> dummyFunctionName{};
public:
  NewAssemblyEmitter(llvm::raw_ostream *fout, std::vector<std::string> dummyFunctionName):
    fout(fout),
    dummyFunctionName(dummyFunctionName)
    {}
  void run(llvm::Module *M);
};

#endif
