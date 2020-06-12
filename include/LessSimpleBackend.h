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

#define POS_STACK 1
#define POS_UNKNOWN 0
#define POS_HEAP -1
#define POS_UNINIT -2

class LessSimpleBackend : public llvm::PassInfoMixin<LessSimpleBackend> {
  std::string outputFile;
  std::string tempPrefix;
  bool printDepromotedModule;
  std::map<llvm::Instruction*, llvm::Value*> stackMap;
  //std::map<llvm::Instruction*, llvm::Instruction*> stackMapRev;
  llvm::Function *spOffset;
  llvm::Function *spSub;
  llvm::Function *rstH;
  llvm::Function *rstS;
  llvm::Function *malloc;
  llvm::Function *main;
  std::map<llvm::Value*, unsigned int> globalVarMap;
  std::set<llvm::Instruction*> loadOperandsSet;
  std::set<llvm::Instruction*> putOnRegsSet;
  std::set<llvm::Instruction*> resumeRegsSet;
  class Registers;
  class StackFrame;
  Registers *regs;
  StackFrame *frame;  
  void depromoteReg(llvm::Function &F);
  //void depromoteReg_BB(llvm::BasicBlock &B);
  int getAccessPos(llvm::Value *V);
  llvm::Function* getSpOffsetFn();
  void removeInst(llvm::Instruction *I);
  void loadRelatedOperands(
    llvm::Instruction *tempI,
    std::vector<llvm::Instruction*> &relatedInstList
  );
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
  llvm::Instruction *depPhi(llvm::PHINode *PI);
  void __depPhi(llvm::PHINode *PI, llvm::Instruction *realValPos);
  void depPhi(llvm::Function &F);
  void depGEP(llvm::GetElementPtrInst *GEPI);
  void depGEP(llvm::Function &F);
  void depGV();
  int insertRst(llvm::BasicBlock &BB);
  void insertRst_first(
    llvm::BasicBlock &BB,
    std::map<llvm::BasicBlock*, int> accessPosMap);
  void insertRst(llvm::Function &F);
  void regAlloc(llvm::BasicBlock &BB, std::set<llvm::BasicBlock*> &BBvisited);
  void _regAlloc(llvm::Function &F);
  void regAlloc(llvm::Function &F);
  void placeSpSub(llvm::Function &F);
  void buildGVMap();
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