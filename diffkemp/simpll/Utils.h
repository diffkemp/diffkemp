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

#include <llvm/IR/Function.h>

using namespace llvm;

/// Enumeration type to easy distinguishing between the compared programs.
enum Program {
    First, Second
};

typedef std::pair<Function *, Function *> FunPair;

/// Extract called function from a called value
const Function *getCalledFunction(const Value *CalledValue);

/// Get name of a type.
std::string typeName(const Type *Type);

/// Delete alias to a function
void deleteAliasToFun(Module &Mod, const Function *Fun);

#endif //DIFFKEMP_SIMPLL_UTILS_H