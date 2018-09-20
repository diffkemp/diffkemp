//===------------------- Utils.cpp - Utility functions --------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementations of utility functions.
///
//===----------------------------------------------------------------------===//

#include "Utils.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Support/raw_ostream.h>
#include <set>

/// Extract called function from a called value Handles situation when the
/// called value is a bitcast.
const Function *getCalledFunction(const Value *CalledValue) {
    const Function *fun = dyn_cast<Function>(CalledValue);
    if (!fun) {
        if (auto BitCast = dyn_cast<BitCastOperator>(CalledValue)) {
            fun = dyn_cast<Function>(BitCast->getOperand(0));
        }
    }
    return fun;
}

/// Get name of a type so that it can be used as a variable in Z3.
std::string typeName(const Type *Type) {
    std::string result;
    raw_string_ostream rso(result);
    Type->print(rso);
    rso.str();
    // We must do some modifications to the type name so that is is usable as
    // a Z3 variable
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    std::replace(result.begin(), result.end(), '(', '$');
    std::replace(result.begin(), result.end(), ')', '$');
    std::replace(result.begin(), result.end(), ',', '_');
    return result;
}

/// Find alias which points to the given function and delete it.
void deleteAliasToFun(Module &Mod, const Function *Fun) {
    std::vector<GlobalAlias *> toRemove;
    for (auto &alias : Mod.aliases()) {
        if (alias.getAliasee() == Fun)
            toRemove.push_back(&alias);
    }
    for (auto &alias : toRemove)
        alias->eraseFromParent();
}

/// Check if the substring behind the last dot ('.') contains only numbers.
bool hasSuffix(std::string Name) {
    return Name.find_last_not_of("0123456789.") < Name.find_last_of(".");
}

/// Remove everything behind the last dot ('.'). Assumes that hasSuffix returned
/// true for the name.
std::string dropSuffix(std::string Name) {
    return Name.substr(0, Name.find_last_of('.'));
}
