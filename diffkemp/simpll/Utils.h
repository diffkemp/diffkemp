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

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>

using namespace llvm;

/// Enumeration type to easy distinguishing between the compared programs.
enum Program {
    First, Second
};

typedef std::pair<Function *, Function *> FunPair;

/// Type for call stack entry: contains the called function and its call
/// location (file and line).
struct CallInfo {
    Function *fun;
    std::string file;
    unsigned line;

    CallInfo(Function *fun, const std::string &file, unsigned int line)
            : fun(fun), file(file), line(line) {}
};
/// Call stack - list of call entries
typedef std::vector<CallInfo> CallStack;

/// Extract called function from a called value
const Function *getCalledFunction(const Value *CalledValue);

/// Get name of a type.
std::string typeName(const Type *Type);

/// Delete alias to a function
void deleteAliasToFun(Module &Mod, Function *Fun);

/// Check if an LLVM name has a .<NUMBER> suffix.
bool hasSuffix(std::string Name);

/// Drop the .<NUMBER> suffix from the LLVM name.
std::string dropSuffix(std::string Name);

/// Get absolute path to a file in which the given function is defined.
/// Requires debug info to work correctly.
std::string getFileForFun(Function *Fun);

/// Get call stack for calling Dest from Src
CallStack getCallStack(Function &Src, Function &Dest);

/// Check if function has side-effect.
bool hasSideEffect(const Function &Fun);

/// Check if the function is an allocator
bool isAllocFunction(const Function &Fun);

/// Inline the given function
void inlineFunction(Module &Mod, Function *InlineFun);

/// Run simplification passes on the function
///  - simplify CFG
///  - dead code elimination
void simplifyFunction(Function *Fun);

/// Get value of the given constant as a string
std::string valueAsString(const Constant *Val);

/// Extract struct type of the value.
/// Works if the value is of pointer type which can be even bitcasted.
StructType *getStructType(const Value *Value);

/// Removes empty attribute sets from an attribute list.
AttributeList cleanAttributeList(AttributeList AL);

#endif //DIFFKEMP_SIMPLL_UTILS_H