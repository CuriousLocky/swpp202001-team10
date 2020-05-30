#include "allocType.h"
#include "llvm/IR/Instructions.h"

using namespace std;
using namespace llvm;

static bool isMallocCall(const CallInst *CI) {
  // Assume that it is undefined behavior for a program to define a
  // 'malloc' function.
  // Actually this is what C standard says. :) "malloc" is categorized as a
  // 'reserved keyword', so defining a "malloc" function is undefined behavior,
  // even if compiler may not complain about it at all.
  //
  // https://www.gnu.org/software/libc/manual/html_node/Reserved-Names.html
  //
  // C17 standard 7.1.3.2:
  //   If the program declares or defines an identifier in a context in which
  //   it is reserved (other than as allowed by 7.1.4), or defines a reserved
  //   identifier as a macro name, the behavior is undefined.
  return CI->getCalledFunction()->getName() == "malloc";
}

// A very simple getBlockType().
// Sees which memory area V is pointing to.
AllocType getBlockType(const Value *V) {
  if (auto *CI = dyn_cast<CallInst>(V)) {
    if (isMallocCall(CI))
      return HEAP;
  } else if (auto *AI = dyn_cast<AllocaInst>(V)) {
    return STACK;
  } else if (auto *BI = dyn_cast<BitCastInst>(V)) {
    return getBlockType(BI->getOperand(0));
  } else if (auto *GI = dyn_cast<GetElementPtrInst>(V)) {
    return getBlockType(GI->getPointerOperand());
  }
  return UNKNOWN;
}