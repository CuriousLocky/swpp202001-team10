#include "LessSimpleBackend.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include <cmath>
#include <regex>
#include <sstream>

using namespace llvm;
using namespace std;

namespace {

// Translates cast destination registers into real registers
map<std::string, std::string> castDestReg{};

string getAccessSizeInStr(Type *T) {
  return std::to_string(getAccessSize(T));
}

string to_string(const ConstantInt &CI) {
  return std::to_string(CI.getZExtValue());
}

bool starts_with(const string &a, const string &prefix) {
  if (a.length() < prefix.length())
    return false;
  return a.substr(0, prefix.length()) == prefix;
}
bool ends_with(const string &a, const string &suffix) {
  if (a.length() < suffix.length())
    return false;
  return a.substr(a.length() - suffix.length(), a.length()) == suffix;
}

void raiseError(const string &msg, Value *V) {
  if (V == nullptr)
    errs() << "AssemblyEmitter: " << msg << "\n";
  else
    errs() << "AssemblyEmitter: " << msg << ": " << *V << "\n";
  abort();
}
void raiseError(Instruction &I) {
  raiseError("Unsupported Instruction", &I);
}
void raiseErrorIf(bool cond, const string &msg, Value *V) {
  if (cond)
    raiseError(msg, V);
}
void raiseErrorIf(bool cond, const string &msg) {
  if (cond)
    raiseError(msg, nullptr);
}

set<unsigned> ValidIntBitwidths = { 1, 8, 16, 32, 64 };

void checkRegisterType(Instruction *I) {
  raiseErrorIf(!I->hasName(), "the instruction does not have a name", I);
  auto *T = I->getType();

  raiseErrorIf(!isa<IntegerType>(T) && !isa<PointerType>(T),
                "unknown register type", I);

  string name = I->getName().str();
  unsigned bw = T->isIntegerTy() ? T->getIntegerBitWidth() : 64;
}

string getRegisterNameFromArgument(Argument *A) {
  const string msg = "incorrect argument name";
  raiseErrorIf(!A->hasName(), msg, A);

  regex re("__arg([0-9]+)__");
  cmatch m;
  string name = A->getName().str();
  regex_match(name.c_str(), m, re);
  raiseErrorIf(m.empty() || m[0].length() != name.length(), msg, A);

  int regid = stoi(m[1].str());
  raiseErrorIf(regid <= 0 || 16 < regid, std::to_string(regid), A);

  // Well, allow names like arg01...
  return "arg" + std::to_string(regid);
}

bool shouldBeMappedToAssemblyRegister(Instruction *I) {
  string name = I->getName().str();
  regex re("^r([0-9]+)_.*");
  cmatch m;
  regex_match(name.c_str(), m, re);
  return !m.empty();
}

string getRegisterNameFromInstruction(Instruction *I, std::string tempPrefix) {
  const string msg = "incorrect instruction name";
  raiseErrorIf(!I->hasName(), msg, I);

  string name = I->getName().str();

  bool is_temp = starts_with(name, tempPrefix);
  if (is_temp) {
    if (castDestReg.count(name) > 0) {
        return castDestReg.at(name);
    }
    return name;
  }

  regex re("^r([0-9]+)_.*");
  cmatch m;
  regex_match(name.c_str(), m, re);
  raiseErrorIf(m.empty() && !is_temp, msg, I);

  int regid = stoi(m[1].str());
  raiseErrorIf(regid <= 0 || 16 < regid, msg, I);

  return "r" + std::to_string(regid);
}

class AssemblyEmitterImpl : public InstVisitor<AssemblyEmitterImpl> {
public:
// NOTE: declaration of members
  vector<string> FnBody;
  string resetHeapName;
  string resetStackName;
  string spSubName;
  string spOffsetName;
  string regSwitchName;
  string tempPrefix;
  llvm::raw_ostream *fout;

private:
  // For resolving bit_cast_ptr and offset
  unordered_map<std::string, int> nameOffsetMap;
  unordered_map<std::string, std::pair<std::string, unsigned int>> ptrResolver;
  unordered_map<llvm::Value*, std::string> sextResolver;
  unordered_map<llvm::Value*, std::pair<std::string, unsigned int>> GEPResolver;
  map<llvm::Value*, std::vector<std::string>> postOpResolver;

  // ----- Emit functions -----
  void _emitAssemblyBody(const string &Cmd, const vector<string> &Ops,
                         stringstream &ss) {
    ss << Cmd;
    for (auto &Op : Ops)
      ss << " " << Op;
  }

  void emitAssembly(const string &DestReg, const string &Cmd,
                    const vector<string> &Ops) {
    stringstream ss;
    ss << DestReg << " = ";
    _emitAssemblyBody(Cmd, Ops, ss);
    FnBody.push_back(ss.str());
  }

  void emitAssembly(const string &Cmd, const vector<string> &Ops) {
    stringstream ss;
    _emitAssemblyBody(Cmd, Ops, ss);
    FnBody.push_back(ss.str());
  }

  void emitAssembly(const string &Cmd) {
    FnBody.push_back(Cmd);
  }

  void emitCopy(const string &DestReg, const string &val) {
    if (DestReg != val)
      emitAssembly(DestReg, "mul", {val, "1", "64"});
  }

  void emitBasicBlockStart(const string &name) {
    FnBody.push_back(name + ":");
  }

  // If V is a register or constant, return its name.
  // If V is alloca, return its offset from stack.
  pair<string, int> getOperand(Value *V, bool shouldNotBeStackSlot = true) {
    if (auto *A = dyn_cast<Argument>(V)) {
      // Its name should be __argN__.
      // Extract & return the argN.
      auto *ATy = A->getType();
      raiseErrorIf(!((ATy->isIntegerTy()) || ATy->isPointerTy()),
                   "unknown argument type", A);
      return { getRegisterNameFromArgument(A), -1 };

    }

    else if (auto *CI = dyn_cast<ConstantInt>(V)) {
      return { to_string(*CI), -1 };
    }

    else if (isa<ConstantPointerNull>(V)) {
      return { "0", -1 };
    }

    else if (auto *CE = dyn_cast<ConstantExpr>(V)) {
      if (CE->getOpcode() == Instruction::IntToPtr) {
        auto *CI = dyn_cast<ConstantInt>(CE->getOperand(0));
        if (CI)
          return { to_string(*CI), -1 };
        else
          assert(false && "Unknown inttoptr form");
      }
      else if (CE->getOpcode() == Instruction::GetElementPtr) {
        if (ptrResolver.count(CE->getName().str()) > 0) {
          auto tmp = ptrResolver.at(CE->getName().str());
          return { tmp.first, tmp.second };
        } else {
          raiseError("GEP not handled", CE);
        }
      }
      assert(false && "Unknown constantexpr");
    }

    else if (auto *I = dyn_cast<Instruction>(V)) {
      if (isa<TruncInst>(I)) {
        // Trunc is just a wrapper for passing type cheking of IR.
        return getOperand(I->getOperand(0), shouldNotBeStackSlot);
      }
      else if (shouldBeMappedToAssemblyRegister(I)) {
        // Note that alloca can also have a register name.
        //   __r1__ = alloca i32
        // This will be lowered to:
        //   r1 = add sp, <offset>
        checkRegisterType(I);
        return { getRegisterNameFromInstruction(I, tempPrefix), -1 };
      }
      else if (nameOffsetMap.count(I->getName().str())) {
        int offset = nameOffsetMap.at(I->getName().str());
        return { "sp", offset };
      }
      else if (castDestReg.count(I->getName().str())) {
        return { castDestReg.at(I->getName().str()), -1 };
      }
      else if (ptrResolver.count(I->getName().str())) {
        auto tmp = ptrResolver.at(I->getName().str());
        return { tmp.first, tmp.second };
      }
      else if (sextResolver.count(I)) {
        return { sextResolver.at(I), -1 };
      }
      else if (GEPResolver.count(I)) {
        auto tmp = GEPResolver.at(I);
        return { tmp.first, tmp.second };
      }
      else if (starts_with(I->getName().str(), tempPrefix)) {
      /* If this is an instruction start with temp and not resolved in
      nameOffsetMap or castDestReg, then  */
        if(dyn_cast<CastInst>(I)){
          return getOperand(I->getOperand(0));
        }
        if(CallInst *CI = dyn_cast<CallInst>(I)){
          visitCallInst(*CI);
          return getOperand(I);
        }
        raiseError("Unresolved found here!", I);
      }
      else {
        assert(false && "Unknown instruction type!");
      }

    }
    else {
      assert(false && "Unknown value type!");
    }
    assert(false && "Uncatched error!");
  }

void getSize(vector<unsigned> &indices, ArrayType *arr) {
  // Recursively find the size of each dimension for the input array, with the
  // element stored in the ndnarray append at the end
  indices.emplace_back(arr->getArrayNumElements());
  if (arr->getArrayElementType()->isArrayTy()) {
    getSize(indices, dyn_cast<ArrayType>(arr->getArrayElementType()));
  }
  else if (arr->getArrayElementType()->isPointerTy()) {
    indices.emplace_back(8);
  } 
  else {
    // assert(arr->getArrayElementType()->isIntegerTy());
    auto tmp = arr->getArrayElementType()->getIntegerBitWidth();
    indices.emplace_back(tmp == 1 ? tmp : tmp / 8);
  }
}

public:
// NOTE: start of Instvisit implementation
  AssemblyEmitterImpl(std::vector<std::string> dummyFunctionName):
    resetHeapName(dummyFunctionName[0]),
    resetStackName(dummyFunctionName[1]),
    spOffsetName(dummyFunctionName[2]),
    spSubName(dummyFunctionName[3]),
    regSwitchName(dummyFunctionName[4]),
    tempPrefix(dummyFunctionName[5])
   {}

  void visitFunction(Function &F) {
    // CurrentStackFrame = StackFrame();
    castDestReg = {};
    nameOffsetMap = {};
    ptrResolver = {};
    sextResolver = {};
    GEPResolver = {};
    postOpResolver = {};
    FnBody.clear();
  }

  void visitBasicBlock(BasicBlock &BB) {
    raiseErrorIf(!BB.hasName(), "This basic block does not have name: ", &BB);
    emitBasicBlockStart(BB.getName().str());
  }

  // Unsupported instruction goes here.
  void visitInstruction(Instruction &I) {
    // Instructions that are eliminated by LessSimpleBackend:
    // - phi
    // - alloca
    raiseError(I);
  }

  // ---- Memory operations ----
  void visitLoadInst(LoadInst &LI) {
    auto [PtrOp, StackOffset] = getOperand(LI.getPointerOperand(), false);
    string Dest = getRegisterNameFromInstruction(&LI, tempPrefix);
    string sz = getAccessSizeInStr(LI.getType());

    if (StackOffset != -1) {
      if (starts_with(PtrOp, tempPrefix))
        emitAssembly(Dest, "load", {sz, "sp", std::to_string(StackOffset)});
      else
        emitAssembly(Dest, "load", {sz, PtrOp, std::to_string(StackOffset)});
    }
    else
      emitAssembly(Dest, "load", {sz, PtrOp, "0"});
  }

  void visitStoreInst(StoreInst &SI) {
    auto [ValOp, _] = getOperand(SI.getValueOperand(), true);
    auto [PtrOp, StackOffset] = getOperand(SI.getPointerOperand(), false);
    string sz = getAccessSizeInStr(SI.getValueOperand()->getType());

    if (StackOffset != -1) {
      if (starts_with(PtrOp, tempPrefix))
        emitAssembly("store", {sz, ValOp, "sp", std::to_string(StackOffset)});
      else
        emitAssembly("store", {sz, ValOp, PtrOp, std::to_string(StackOffset)});
    }
    else
      emitAssembly("store", {sz, ValOp, PtrOp, "0"});
  }

  // ---- Arithmetic operations ----
  void visitBinaryOperator(BinaryOperator &BO) {
    auto [Op1, unused_1] = getOperand(BO.getOperand(0));
    auto [Op2, unused_2] = getOperand(BO.getOperand(1));
    string DestReg = getRegisterNameFromInstruction(&BO, tempPrefix); // iN -> i64
    string Cmd;
    string sz = std::to_string(BO.getType()->getIntegerBitWidth());

    switch(BO.getOpcode()) {
    case Instruction::UDiv: Cmd = "udiv"; break;
    case Instruction::SDiv: Cmd = "sdiv"; break;
    case Instruction::URem: Cmd = "urem"; break;
    case Instruction::SRem: Cmd = "srem"; break;
    case Instruction::Mul:  Cmd = "mul"; break;
    case Instruction::Shl:  Cmd = "shl"; break;
    case Instruction::AShr: Cmd = "ashr"; break;
    case Instruction::LShr: Cmd = "lshr"; break;
    case Instruction::And:  Cmd = "and"; break;
    case Instruction::Or:   Cmd = "or"; break;
    case Instruction::Xor:  Cmd = "xor"; break;
    case Instruction::Add:  Cmd = "add"; break;
    case Instruction::Sub:  Cmd = "sub"; break;
    default: raiseError(BO); break;
    }

    emitAssembly(DestReg, Cmd, {Op1, Op2, sz});
  }
  void visitICmpInst(ICmpInst &II) {
    auto [Op1, unused_1] = getOperand(II.getOperand(0));
    auto [Op2, unused_2] = getOperand(II.getOperand(1));
    string DestReg = getRegisterNameFromInstruction(&II, tempPrefix); // i1 -> i64
    string pred = ICmpInst::getPredicateName(II.getPredicate()).str();
    auto *OpTy = II.getOperand(0)->getType();
    string sz = std::to_string(
      OpTy->isPointerTy() ? 64 : OpTy->getIntegerBitWidth());

    emitAssembly(DestReg, "icmp", {pred, Op1, Op2, sz});
  }
  void visitSelectInst(SelectInst &SI) {
    auto [Op1, unused_1] = getOperand(SI.getOperand(0));
    auto [Op2, unused_2] = getOperand(SI.getOperand(1));
    auto [Op3, unused_3] = getOperand(SI.getOperand(2));
    string DestReg = getRegisterNameFromInstruction(&SI, tempPrefix);
    emitAssembly(DestReg, "select", {Op1, Op2, Op3});
  }
  void visitGetElementPtrInst(GetElementPtrInst &GEPI) {
  /*
  <result> = getelementptr <ty>, <ty>* <ptrval>{, [inrange] <ty> <idx>}*
  <result> = getelementptr inbounds <ty>, <ty>* <ptrval>{, [inrange] <ty> <idx>}*
  */
    // Get Pointer Type
    // TODO: very complicated logic here, provide some notes for the sake of
    // readaibility!

    Type *PtrTy = GEPI.getPointerOperandType();

    auto [Ptr, _] = getOperand(GEPI.getOperand(0)); // Pointer Operand

    string DestReg = getRegisterNameFromInstruction(&GEPI, tempPrefix);

    if (PtrTy->getPointerElementType()->isIntegerTy()) {
      auto tmp = PtrTy->getPointerElementType()->getIntegerBitWidth();
      unsigned elementByte = tmp == 1 ? tmp : tmp / 8;
      auto [Of, _] = getOperand(GEPI.getOperand(1));

      if (starts_with(DestReg, tempPrefix)) { // All constants
        GEPResolver.emplace(&GEPI,
          std::pair<std::string, unsigned>{Ptr, stoi(Of)*elementByte});
        return;
      }
      else if (starts_with(DestReg, "r")) { // Register
        emitAssembly(DestReg, "mul", {Of, std::to_string(elementByte), "64"});
        emitAssembly(DestReg, "add", {Ptr, DestReg, "64"});
        GEPResolver.emplace(&GEPI,
          std::pair<std::string, unsigned>{DestReg, -1});
        return;
      }
      else {
        raiseError("Invalid destination register", &GEPI);
      }
    }

    else if (PtrTy->getPointerElementType()->isPointerTy()) {
      raiseErrorIf(GEPI.getNumIndices() != 1, "Too many indices", &GEPI);
      auto [Of, unused_2] = getOperand(GEPI.getOperand(1));
      string DestReg = getRegisterNameFromInstruction(&GEPI, tempPrefix);

      if (starts_with(DestReg, tempPrefix)) {
        GEPResolver.emplace(&GEPI,
         std::pair<std::string, unsigned>{Ptr, stoi(Of)*8});
      }
      else if (starts_with(DestReg, "r")) { // Register
        emitAssembly(DestReg, "mul", {Of, "8", "64"});
        emitAssembly(DestReg, "add", {Ptr, DestReg, "64"});
        GEPResolver.emplace(&GEPI,
          std::pair<std::string, unsigned>{DestReg, -1});
        return;
      }
      else {
        raiseError("Invalid destination register", &GEPI);
      }
    }

    else if (PtrTy->getPointerElementType()->isArrayTy()) { // Array pointer
      vector<unsigned> size;
      getSize(size, dyn_cast<ArrayType>(PtrTy->getPointerElementType()));
      unsigned elementByte = size.back();

      if (starts_with(DestReg, tempPrefix)) { // All constants
        unsigned offset = 0;
        vector<unsigned> indices;

        for (unsigned i = 1; i < GEPI.getNumOperands(); i++) {
          auto [Of, _] = getOperand(GEPI.getOperand(i));
          indices.emplace_back(stoi(Of));
        }
        assert(indices.size() <= size.size());

        for (unsigned i = 0; i < indices.size(); i++) {
          offset += indices[i];
          offset *= size[i];
        }

        for (unsigned j = indices.size(); j < size.size(); j++) {
          offset *= size[j];
        }

        GEPResolver.emplace(&GEPI,
                            std::pair<std::string, unsigned>{Ptr, offset});
      }

      else if (starts_with(DestReg, "r")) { // Contains registers in indices
        vector<string> indices;
        unsigned firstRegIdx = 0;

        for (unsigned i = 1; i < GEPI.getNumOperands(); i++) {
          auto [Of, _] = getOperand(GEPI.getOperand(i));
          indices.emplace_back(Of);
          if(starts_with(Of, "r") || starts_with(Of, "arg")) {
            firstRegIdx = i-1;
          }
        }
        assert(indices.size() <= size.size());

        unsigned ini = 0;

        for (unsigned i = 0; i < firstRegIdx; i++) {
          ini += stoi(indices[i]);
          ini *= size[i];
        }

        /* Avoid add 0 and mul 0 */
        if (ini) {
          emitAssembly(DestReg, "add",
                       { std::to_string(ini), indices[firstRegIdx] ,"64" });
          emitAssembly(DestReg, "mul",
                       { DestReg, std::to_string(size[firstRegIdx]), "64"} );
        } else {
          emitAssembly(DestReg, "mul",
                    { indices[firstRegIdx], std::to_string(size[firstRegIdx]), "64"});
        }

        for (unsigned j = firstRegIdx+1; j < indices.size(); j++) {
          emitAssembly(DestReg, "add",
                    { DestReg, indices[j] ,"64" });
          emitAssembly(DestReg, "mul",
                    { DestReg, std::to_string(size[j]), "64"});
        }
        
        for (unsigned k = indices.size(); k < size.size(); k++) {
          emitAssembly(DestReg, "mul",
                    { DestReg, std::to_string(size[k]), "64"});
        }
        emitAssembly(DestReg, "add", {DestReg, Ptr, "64"});
        GEPResolver.emplace(&GEPI,
                            std::pair<std::string, unsigned>{DestReg, -1});
      }
      else {
        raiseError("Invalid destination register!", &GEPI);
      }
    }

    else {
      raiseError("Unsuported pointer type", &GEPI);
    }
  }

  // ---- Casts ----
  void visitZExtInst(ZExtInst &ZI) {
    // This test should pass.
    auto DestReg = getRegisterNameFromInstruction(&ZI, tempPrefix);
    auto [Op1, offset] = getOperand(ZI.getOperand(0));
    if (starts_with(DestReg, tempPrefix)) { // start with temp, penetrate through
      if (offset != -1) {
        nameOffsetMap.emplace(ZI.getName().str(), offset);
      } else {
        castDestReg.emplace(ZI.getName().str(), Op1);
      }
    } else { // not start with temp
      uint64_t Mask = (1llu << (ZI.getSrcTy()->getIntegerBitWidth())) - 1;
      emitAssembly(DestReg, "and", {Op1, std::to_string(Mask), "64"});
    }
  }

  void visitSExtInst(SExtInst &SI) {
    // Handle this in getOperand
    string DestReg = getRegisterNameFromInstruction(&SI, tempPrefix);
    auto [SrcReg, _] = getOperand(SI.getOperand(0));

    raiseErrorIf(starts_with(DestReg, tempPrefix),
      "Unresolved register name: start with tempPrefix", &SI);

    raiseErrorIf(((!SI.getSrcTy()->isIntegerTy())||
                  (!SI.getDestTy()->isIntegerTy())),
                  "Unsuppored type: not integer", &SI);

    auto to = SI.getDestTy()->getIntegerBitWidth();
    auto from = SI.getSrcTy()->getIntegerBitWidth();
    raiseErrorIf(to - from < 0, "SExt to smaller integer type!", &SI);

    string facToMul = std::to_string((uint64_t)pow(2, to - from));
    string bitToShift = std::to_string(to - from);
    emitAssembly(DestReg, "mul", {SrcReg, facToMul, std::to_string(to)});
    // emitAssembly(DestReg, "shl", {SrcReg, bitToShift, std::to_string(to)});
    emitAssembly(DestReg, "ashr", {DestReg, bitToShift, std::to_string(to)});
    sextResolver.emplace(&SI, DestReg);
  }

  void visitTruncInst(TruncInst &TI) {
    // This test should pass.
    if (auto *I = dyn_cast<Instruction>(TI.getOperand(0))) {
      (void)getRegisterNameFromInstruction(I, tempPrefix);
      auto [Op1, offset] = getOperand(TI.getOperand(0));
      if (offset != -1) {
        nameOffsetMap.emplace(TI.getName().str(), offset);
      } else {
        castDestReg.emplace(TI.getName().str(), Op1);
      }
      return;
    }
    raiseError("Not truncing an instruction, what are you doing", &TI);
  }
  // * Support for bitCast + spOffset
  void visitBitCastInst(BitCastInst &BCI) {
    auto [Op1, offset] = getOperand(BCI.getOperand(0));
    if (offset != -1) {
      nameOffsetMap.emplace(BCI.getName().str(), offset);
    } else {
      castDestReg.emplace(BCI.getName().str(), Op1);
    }
  }

  void visitPtrToIntInst(PtrToIntInst &PI) {
    auto [Op1, offset] = getOperand(PI.getOperand(0));
    if (offset != -1) {
      nameOffsetMap.emplace(PI.getName().str(), offset);
    } else {
      castDestReg.emplace(PI.getName().str(), Op1);
    }
  }
  void visitIntToPtrInst(IntToPtrInst &II) {
    auto [Op1, offset] = getOperand(II.getOperand(0));
    if (offset != -1) {
      nameOffsetMap.emplace(II.getName().str(), offset);
    } else {
      castDestReg.emplace(II.getName().str(), Op1);
    }
  }

  // ---- Call ----
  // TODO: handle dummyFunction swith register
  // handle dummy function here!
  void visitCallInst(CallInst &CI) {
    string FnName = CI.getCalledFunction()->getName().str();
    vector<string> Args;
    bool MallocOrFree = true;
    if (FnName == resetStackName) {
      Args.emplace_back("stack");
      emitAssembly("reset", Args);
      return;
    }
    if (FnName == resetHeapName) {
      Args.emplace_back("heap");
      emitAssembly("reset", Args);
      return;
    }
    // handle spOffset here!
    if (FnName == spOffsetName) {
      string DestReg = getRegisterNameFromInstruction(&CI, tempPrefix);
      string offset = getOperand(CI.getArgOperand(0)).first;
      if (starts_with(DestReg, tempPrefix)) {
        nameOffsetMap.emplace(DestReg, stoi(offset));
      } else {
        Args.emplace_back("sp");
        Args.emplace_back(offset);
        Args.emplace_back("64");
        emitAssembly(DestReg, "add", Args);
      }
      // Do not emit assembly
      return;
    }
    // handle spSub here!
    if (FnName == spSubName) {
      string frameSize = getOperand(CI.getArgOperand(0)).first;
      if (!stoi(frameSize)) {
        return;
      } else {
        Args.emplace_back("sp");
        Args.emplace_back(frameSize);
        Args.emplace_back("64");
        emitAssembly("sp", "sub", Args);
        return;
      }
    }
    // handle regSwitch here!
    if (FnName == regSwitchName) {
      return;
    }

    if (FnName != "malloc" && FnName != "free") {
      MallocOrFree = false;
      Args.emplace_back(FnName);
    }

    unsigned Idx = 0;
    for (auto I = CI.arg_begin(), E = CI.arg_end(); I != E; ++I) {
      string name = getOperand(*I).first;
      if (castDestReg.count(name) > 0) {
          name = castDestReg.at(name);
      }
      Args.emplace_back(name);
      ++Idx;
    }
    if (CI.hasName()) {
      string DestReg = getRegisterNameFromInstruction(&CI, tempPrefix);
      emitAssembly(DestReg, MallocOrFree ? FnName : "call", Args);
    } else {
      emitAssembly(MallocOrFree ? FnName : "call", Args);
    }
  }

  // ---- Terminators ----
  void visitReturnInst(ReturnInst &RI) {
    if (RI.getReturnValue() == nullptr) {
      emitAssembly("ret", {});
      return;
    }
    emitAssembly("ret", { getOperand(RI.getReturnValue()).first });
  }
  void visitBranchInst(BranchInst &BI) {
    if (BI.isUnconditional()) {
      emitAssembly("br", {(string)BI.getSuccessor(0)->getName()});
    } else {
      string msg = "Branch condition should be icmp ne";
      auto *BCond = BI.getCondition();
      raiseErrorIf(!isa<ICmpInst>(BCond), msg, BCond);

      auto *II = dyn_cast<ICmpInst>(BCond);

      auto Cond = getRegisterNameFromInstruction(II, tempPrefix);
      emitAssembly("br", { Cond, (string)BI.getSuccessor(0)->getName(),
                                 (string)BI.getSuccessor(1)->getName()});
    }
  }
  void visitSwitchInst(SwitchInst &SI) {
    auto [Cond, _] = getOperand(SI.getCondition());
    vector<string> Args;
    Args.emplace_back(Cond);
    for (SwitchInst::CaseIt I = SI.case_begin(), E = SI.case_end();
         I != E; ++I) {
      Args.push_back(to_string(*I->getCaseValue()));
      Args.push_back(I->getCaseSuccessor()->getName().str());
    }
    Args.push_back(SI.getDefaultDest()->getName().str());
    emitAssembly("switch", Args);
  }
};

};

void NewAssemblyEmitter::run(Module *DepromotedM) {
  AssemblyEmitterImpl Em(dummyFunctionName);
  unsigned TotalStackUsage = 0;
  for (auto &F : *DepromotedM) {
    if (F.isDeclaration())
      continue;

    Em.visit(F);
    *fout << "\n";
    *fout << "; Function " << F.getName() << "\n";
    *fout << "start " << F.getName() << " " << F.arg_size() << ":\n";

    assert(Em.FnBody.size() > 0);

    *fout << "  " << Em.FnBody[0] << "\n";

    for (unsigned i = 1; i < Em.FnBody.size(); ++i) {
      assert(Em.FnBody[i].size() > 0);
      if (Em.FnBody[i][0] == '.')
        // Basic block
        *fout << "\n  " << Em.FnBody[i] << "\n";

      else
        // Instruction
        *fout << "    " << Em.FnBody[i] << "\n";
    }

    *fout << "end " << F.getName() << "\n";
  }
}
