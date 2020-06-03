#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/Scalar/SROA.h"
#include "llvm/Transforms/Scalar/ADCE.h"

#include <string>

using namespace std;
using namespace llvm;


static cl::OptionCategory optCategory("irgen options");

static cl::opt<string> optInput(
    cl::Positional, cl::desc("<input bitcode file>"), cl::Required,
    cl::value_desc("filename"), cl::cat(optCategory));

static cl::opt<string> optOutput(
    cl::Positional, cl::desc("output ll file"), cl::Required,
    cl::value_desc("filename"), cl::cat(optCategory));

static llvm::ExitOnError ExitOnErr;

static Function *MainFunc;


// adapted from llvm-dis.cpp
static unique_ptr<Module> openInputFile(LLVMContext &Context,
                                        string InputFilename) {
  auto MB = ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(InputFilename)));
  SMDiagnostic Diag;
  auto M = getLazyIRModule(move(MB), Diag, Context, true);
  if (!M) {
    Diag.print("", errs(), false);
    return 0;
  }
  ExitOnErr(M->materializeAll());
  return M;
}

class GEPOffsetToI64 : public llvm::PassInfoMixin<GEPOffsetToI64> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    LLVMContext &Cxt = F.getParent()->getContext();
    auto *I64 = IntegerType::get(Cxt, 64);
    for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      auto *GEP = dyn_cast<GetElementPtrInst>(&*I);
      if (!GEP)
        continue;

      for (auto &U: GEP->indices()) {
        Value *V = U.get();
        if (!V->getType()->isIntegerTy()) {
          errs() << "ERROR: getelementptr's index should be i64!\n";
          errs() << "\t" << *V << "\n";
          exit(1);
        } else if (V->getType()->getIntegerBitWidth() == 64)
          continue;

        if (auto *CI = dyn_cast<ConstantInt>(V)) {
          U.set(ConstantInt::get(I64, CI->getSExtValue()));
        } else {
          errs() << "ERROR: getelementptr's index should be i64!\n";
          errs() << "\t" << *V << "\n";
          exit(1);
        }
      }
    }
    return PreservedAnalyses::all();
  }
};

class RemoveUnsupportedOps : public llvm::PassInfoMixin<RemoveUnsupportedOps> {
  Constant *getZero(Type *T) {
    return T->isPointerTy() ?
        ConstantPointerNull::get(dyn_cast<PointerType>(T)) :
        ConstantInt::get(T, 0);
  }

public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    vector<Instruction *> V;
    LLVMContext &Cxt = F.getParent()->getContext();
    for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      Instruction *II = &*I;
      bool Deleted = false;
      if (auto *Intr = dyn_cast<IntrinsicInst>(II)) {
        if (Intr->getIntrinsicID() == Intrinsic::lifetime_start ||
            Intr->getIntrinsicID() == Intrinsic::lifetime_end) {
          V.push_back(Intr);
          Deleted = true;
        }
      } else if (auto *CI = dyn_cast<CallInst>(II)) {
        auto *CF = CI->getCalledFunction();
        if (CF->getName() == "read") {
          if (!CF->getReturnType()->isIntegerTy() ||
              CF->getReturnType()->getIntegerBitWidth() != 64) {
            errs() << "ERROR: read() should return i64!\n";
            exit(1);
          } else if (CF->arg_size() != 0) {
            errs() << "ERROR: read() does not take any argument!\n";
            exit(1);
          }
        } else if (CF->getName() == "write") {
          if (!CF->getReturnType()->isVoidTy()) {
            errs() << "ERROR: write() should return void!\n";
            exit(1);
          } else if (CF->arg_size() != 1) {
            errs() << "ERROR: write() takes a single i64 argument!\n";
            exit(1);
          }
          auto &A = *CF->arg_begin();
          if (!A.getType()->isIntegerTy() ||
              A.getType()->getIntegerBitWidth() != 64) {
            errs() << "ERROR: write() takes a single i64 argument!\n";
            exit(1);
          }
        } else if (CF->isDeclaration()) {
          errs() << "ERROR: " << CF->getName() << " has no body!\n";
          errs() << *CI << "\n";
          exit(1);
        }
      } else if (auto *UI = dyn_cast<UnreachableInst>(II)) {
        auto *RE = F.getReturnType()->isVoidTy() ? ReturnInst::Create(Cxt) :
            ReturnInst::Create(Cxt, getZero(F.getReturnType()));
        RE->insertBefore(UI);
        V.push_back(UI);
        Deleted = true;
      }

      if (!Deleted) {
        for (unsigned i = 0; i < II->getNumOperands(); ++i) {
          Value *V = II->getOperand(i);
          if (isa<UndefValue>(V)) {
            auto *T = V->getType();
            II->setOperand(i, getZero(T));
          }
        }
      }
    }

    for (auto *I : V)
      I->eraseFromParent();
    return PreservedAnalyses::all();
  }
};

class CheckConstExpr : public llvm::PassInfoMixin<CheckConstExpr> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    for (auto &G : M.globals()) {
      if (auto *F = dyn_cast<Function>(&G)) {
        for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
          Instruction *II = &*I;
          for (auto &V : I->operands()) {
            if (isa<ConstantExpr>(V)) {
              errs() << "ERROR: Constant expressions should not exist!\n";
              errs() << "\t" << *I << "\n";
              exit(1);
            }
          }
        }
      } else if (auto *GV = dyn_cast<GlobalVariable>(&G)) {
        if (GV->hasInitializer()) {
          if (GV->getValueType()->isIntegerTy()) {
            new StoreInst(GV->getInitializer(), GV,
                          &*MainFunc->getEntryBlock().begin());
          } else {
            errs() << "WARNING: Global variables should not have initializers!\n";
            errs() << "\t" << *GV << "\n";
          }
        }
      }
    }
    return PreservedAnalyses::all();
  }
};

class AddNoAliasToMalloc : public llvm::PassInfoMixin<AddNoAliasToMalloc> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    for (auto &F : M) {
      if (F.isDeclaration()) {
        if (F.getName() == "malloc")
          F.addAttribute(AttributeList::ReturnIndex, Attribute::NoAlias);
        continue;
      }

      if (F.getName() == "main") {
        MainFunc = &F;
      }

      for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        auto *CI = dyn_cast<CallInst>(&*I);
        if (!CI) continue;
        if (CI->getCalledFunction()->getName() == "malloc" &&
            !CI->hasRetAttr(Attribute::NoAlias)) {
          CI->addAttribute(AttributeList::ReturnIndex, Attribute::NoAlias);
        }
      }
    }
    return PreservedAnalyses::all();
  }
};


int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  EnableDebugBuffering = true;

  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext Context;
  // Read the module
  auto M = openInputFile(Context, optInput);
  if (!M)
    return 1;

  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;
  PassBuilder PB;
  // Register all the basic analyses with the managers.
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);


  FunctionPassManager FPM;
  FPM.addPass(SROA());
  FPM.addPass(RemoveUnsupportedOps());
  FPM.addPass(ADCEPass());
  FPM.addPass(GEPOffsetToI64());

  ModulePassManager MPM;
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.addPass(AddNoAliasToMalloc());
  MPM.addPass(CheckConstExpr());

  // Run!
  MPM.run(*M, MAM);

  if (!MainFunc) {
    errs() << "ERROR: No main function\n";
    return 1;
  }

  error_code EC;
  raw_fd_ostream fout(optOutput, EC);
  fout << *M;

  return 0;
}
