#ifndef LESS_SIMPLE_BACKEND_H
#define LESS_SIMPLE_BACKEND_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <map>
#include <set>

#define REG_SIZE 4

class LessSimpleBackend : public llvm::PassInfoMixin<LessSimpleBackend> {
  std::string outputFile;
  std::string tempPrefix;
  bool printDepromotedModule;
  std::map<llvm::Value*, int> stackMap;
  llvm::Function *spOffset;
  llvm::Function *spSub;
  llvm::Function *rstH;
  llvm::Function *rstS;
  std::set<llvm::Instruction*> loadOperandsSet;
  std::set<llvm::Instruction*> putOnRegsSet;
  std::set<llvm::Instruction*> resumeRegsSet;
  class Registers;
  class StackFrame;
  Registers *regs;
  StackFrame *frame;  
  void depromoteReg(llvm::Function &F);
  //void depromoteReg_BB(llvm::BasicBlock &B);
  llvm::Function* getSpOffsetFn();
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
  void depPhi(llvm::PHINode *I);
  void depPhi(llvm::Function& F);
  void regAlloc(llvm::Function& F);
  void placeSpSub(llvm::Function& F);
public:
  LessSimpleBackend(std::string outputFile, bool printDepromotedModule) :
      outputFile(outputFile), printDepromotedModule(printDepromotedModule) {}
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
  llvm::Function *getSpOffset();
  llvm::Function *getRstH();
  llvm::Function *getRstS();
  std::string getTempPrefix();
};

#endif