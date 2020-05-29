#include "SimpleBackend.h"

#include "llvm/IR/PassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"

/*****************************************************************************/
/***************** Include new header files in this section ******************/
/*****************************************************************************/
#include "llvm/Transforms/IPO/DeadArgumentElimination.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "ArithmeticOptimization.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/IPO/GlobalOpt.h"
#include "Alloca2reg.h"
/*****************************************************************************/
#include <string>

using namespace std;
using namespace llvm;

static cl::OptionCategory optCategory("SWPP Compiler options");

static cl::opt<string> optInput(
    cl::Positional, cl::desc("<input bitcode file>"), cl::Required,
    cl::value_desc("filename"), cl::cat(optCategory));

static cl::opt<string> optOutput(
    "o", cl::desc("output assembly file"), cl::cat(optCategory), cl::Required,
    cl::init("a.s"));

static cl::opt<bool> optPrintDepromotedModule(
    "print-depromoted-module", cl::desc("print depromoted module"),
    cl::cat(optCategory), cl::init(false));

static llvm::ExitOnError ExitOnErr;


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

  // Function-level pass
  FunctionPassManager FPM;
  // If you want to add a function-level pass, add FPM.addPass(MyPass()) here.
  FPM.addPass(ArithmeticOptimization());
  FPM.addPass(GVN());
  FPM.addPass(DCEPass());
  FPM.addPass(Alloca2reg());
  // CGSCC-level pass
  CGSCCPassManager CGPM;
  CGPM.addPass(InlinerPass());

  ModulePassManager MPM;
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.addPass(createModuleToPostOrderCGSCCPassAdaptor(std::move(CGPM)));
  // If you want to add your module-level pass, add MPM.addPass(MyPass2()) here.
  MPM.addPass(DeadArgumentEliminationPass());
  MPM.addPass(GlobalOptPass());
  MPM.addPass(SimpleBackend(optOutput, optPrintDepromotedModule));
  
  MPM.run(*M, MAM);

  return 0;
}
