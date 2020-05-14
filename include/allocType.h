#ifndef ALLOC_TYPE_H
#define ALLOC_TYPE_H

#include "llvm/ADT/Optional.h"
#include "llvm/IR/Value.h"

enum AllocType {
  STACK, HEAP, UNKNOWN
};

AllocType getBlockType(const llvm::Value *V);

#endif