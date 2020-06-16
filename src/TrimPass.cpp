#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "TrimPass.h"
#include <vector>
#include <set>

using namespace llvm;
using namespace std;
using namespace llvm::PatternMatch;

vector <Function *> functions;
set <Function *> used;

void DFS(Function *F) {
    used.insert(F);
    for (auto &BB : *F)  
        for (auto &I : BB)
            if (auto *CB = dyn_cast<CallInst>(&I))
                if (used.count(CB->getCalledFunction()) == 0) 
                    DFS(CB->getCalledFunction());
                    

}

PreservedAnalyses TrimPass::run(Module &M, ModuleAnalysisManager &MAM) {
    Function *main = nullptr;

    for (Function &F : M.getFunctionList()) {
        if (F.getName() == "main") {
            main = &F;
        }
        functions.push_back(&F);
    }
    assert(main != nullptr);
    DFS(main);

    for (int i = 0; i < functions.size(); ++i) {
        if (used.count(functions[i]) == 0) {
            Function *F = functions[i];
            F->replaceAllUsesWith(UndefValue::get((Type*)F->getType()));
            F->eraseFromParent(); 
        }
    }
    //outs () << M;
    return PreservedAnalyses::all();
}
