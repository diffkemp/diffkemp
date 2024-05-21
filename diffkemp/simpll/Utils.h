//===-------------------- Utils.h - Utility functions ---------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of utility functions and enums.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_UTILS_H
#define DIFFKEMP_SIMPLL_UTILS_H

#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <map>
#include <unordered_map>

using namespace llvm;

/// Enumeration type to easy distinguishing between the compared programs.
enum Program { First, Second };

/// A structure containing basic type information: its name and size in bytes
struct TypeInfo {
    StringRef Name;
    uint64_t Size;
};

typedef std::pair<Function *, Function *> FunPair;
typedef std::pair<const Function *, const Function *> ConstFunPair;
typedef std::pair<const GlobalValue *, const GlobalValue *> GlobalValuePair;
typedef std::pair<const CallInst *, const CallInst *> CallPair;

/// Instructions pointer set.
typedef SmallPtrSet<const Instruction *, 32> InstructionSet;

/// Instruction to instruction mapping.
typedef DenseMap<const Instruction *, const Instruction *> InstructionMap;

/// Variable name to debug info type mapping.
typedef std::unordered_map<std::string, const DIType *> LocalVariableMap;

/// Program to string
std::string programName(Program p);

/// Convert a value to a function.
/// Handles situation then the actual function is inside a bitcast or alias.
const Function *valueToFunction(const Value *Value);

/// Extract called function from a called value. Handles situation when the
/// called value is a bitcast.
const Function *getCalledFunction(const CallInst *Call);

/// Extract called function from a called value. Handles situation when the
/// called value is a bitcast.
Function *getCalledFunction(CallInst *Call);

Value *getCallee(CallInst *Call);
const Value *getCallee(const CallInst *Call);

/// Extracts value from an arbitrary number of casts.
const Value *stripAllCasts(const Value *Val);

/// Extracts value from an arbitrary number of casts.
Value *stripAllCasts(Value *Val);

/// Get name of a type.
std::string typeName(const Type *Type);

/// Delete alias to a function
void deleteAliasToFun(Module &Mod, Function *Fun);

/// Check if an LLVM name has a .<NUMBER> suffix.
bool hasSuffix(std::string Name);

/// Drop the .<NUMBER> suffix from the LLVM name.
std::string dropSuffix(std::string Name);

/// Join directory path with a filename in case the filename does not already
/// contain the directory.
std::string joinPath(StringRef DirName, StringRef FileName);

/// Get absolute path to a file in which the given function is defined.
/// Requires debug info to work correctly.
std::string getFileForFun(const Function *Fun);

/// Check if function has side-effect.
bool hasSideEffect(const Function &Fun);

/// Check if the function is an allocator
bool isAllocFunction(const Function &Fun);

/// Retuns true if the given value is a cast (instruction or constant
/// expression)
bool isCast(const Value *Val);

/// Returns true if the given instruction is a GEP with all indices equal to 0
bool isZeroGEP(const Value *Val);

/// Returns true if the given instruction is a boolean negation operation
bool isLogicalNot(const Instruction *Inst);

/// Returns true if the given instruction is a reorderable binary operation,
/// i.e., it is commutative and associative. Note that IEEE 754 floating-point
/// addition/multiplication is NOT associative.
bool isReorderableBinaryOp(const Instruction *Inst);

/// Run simplification passes on the function
///  - simplify CFG
///  - dead code elimination
void simplifyFunction(Function *Fun);

/// Get value of the given constant as a string
std::string valueAsString(const Constant *Val);

/// Removes empty attribute sets from an attribute list.
AttributeList cleanAttributeList(AttributeList AL, LLVMContext &Context);

/// Get a non-const pointer to a call instruction in a function
CallInst *findCallInst(const CallInst *Call, Function *Fun);

/// Gets C source file from a DIScope and the module.
std::string getSourceFilePath(DIScope *Scope);

/// Checks whether the character is valid for a C identifier.
bool isValidCharForIdentifier(char ch);

/// Checks whether the character is valid for the first character of
/// a C identifier.
bool isValidCharForIdentifierStart(char ch);

/// Finds the string given in the second argument and replaces it with the one
/// given in the third argument.
void findAndReplace(std::string &input, std::string find, std::string replace);

/// Convert constant expression to instruction. (Copied from LLVM and modified)
/// Note: this is for constant ConstantExpr pointers; for non-constant ones,
/// the built-in getAsInstruction method is sufficient.
const Instruction *getConstExprAsInstruction(const ConstantExpr *CEx);

/// Retrives type of the value based its C source code identifier.
const DIType *getCSourceIdentifierType(std::string expr,
                                       const Function *Parent,
                                       const LocalVariableMap &LVMap);

/// Generates human-readable C-like identifier for type.
std::string getIdentifierForType(Type *Ty);

/// Finds the name of a value (in case there exists one).
std::string getIdentifierForValue(
        const Value *Val,
        const std::map<std::pair<StructType *, uint64_t>, StringRef>
                &StructFieldNames,
        const Function *Parent = nullptr);

/// Copies properties from one call instruction to another.
void copyCallInstProperties(CallInst *srcCall, CallInst *destCall);

/// Copies properties from one function to another.
void copyFunctionProperties(Function *srcFun, Function *destFun);

/// Tests whether two names of types or globals match. Names match if they
/// are the same or if the DiffKemp pattern name prefixes are used.
bool namesMatch(const StringRef &L, const StringRef &R, bool IsLeftSide);

/// Converts value to its string representation.
/// Note: Currently the only place that calls this is returns.gdb, which lacks
/// the ability to directly dump values because GDB can't call the corresponding
/// methods.
std::string valueToString(const Value *Val);

/// Converts type to its (LLVM IR) string representation.
std::string typeToString(Type *Ty);

/// Get a string matching the current indentation level.
/// \param prefixChar Indentation prefix character, defaults to space.
std::string getDebugIndent(const char prefixChar = ' ');

/// Increase the level of debug indentation by one.
void increaseDebugIndentLevel();

/// Decrease the level of debug indentation by one.
void decreaseDebugIndentLevel();

/// Inline a function call and return true if inlining succeeded.
bool inlineCall(CallInst *Call);

namespace Color {
std::string makeRed(std::string text);
std::string makeGreen(std::string text);
std::string makeYellow(std::string text);
} // namespace Color

/// Return LLVM struct type of the given name
StructType *getTypeByName(const Module &Mod, StringRef Name);

/// Retrieve information about a structured type being pointed to by a value
TypeInfo getPointeeStructTypeInfo(const Value *Val, const DataLayout *Layout);

/// Given an instruction and a pointer value, try to determine whether the
/// instruction may store to the memory pointed to by the pointer. This is
/// currently implemented only as a set of heuristics - if they do not suffice,
/// we return true.
bool mayStoreTo(const Instruction *Inst, const Value *Ptr);

/// Given two pointer values, try do determine whether they may alias. The
/// function currently supports only simple aliasing of local memory.
bool mayAlias(const Value *PtrL, const Value *PtrR);

/// Given an instruction, append metadata with the given kind and value. If the
/// given metadata kind already exists, the value is appended to the existing
/// metadata node.
void appendMetadata(Instruction *Inst,
                    const StringRef kind,
                    const StringRef value);

/// Given a module, its clone, and a function, replace the function in the
/// module by its version in the clone.
void replaceFunctionWithClone(Module *Mod,
                              Module *ModClone,
                              const StringRef FunName);

/// Given a pointer value, return the instruction which allocated the memory
/// where the pointer points. Return a null pointer if the alloca is not found.
const AllocaInst *getAllocaFromPtr(const Value *Ptr);

/// Return the alloca instruction that was used to allocate the variable into
/// which the pointer operand of the given instruction (e.g., load) points.
/// If such an alloca cannot be found, return nullptr.
template <typename InstType>
const AllocaInst *getAllocaOp(const InstType *Inst) {
    return getAllocaFromPtr(Inst->getPointerOperand());
}

#endif // DIFFKEMP_SIMPLL_UTILS_H
