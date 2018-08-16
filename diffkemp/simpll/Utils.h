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

/// Check if a function can transitively call another function.
bool callsTransitively(const Function &Caller, const Function &Callee);

/// Get function called by a Call instruction.
const Function *getCalledFunction(const CallInst *Call);

/// Get name of a type.
std::string typeName(const Type *Type);

#endif //DIFFKEMP_SIMPLL_UTILS_H