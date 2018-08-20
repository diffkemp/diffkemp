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
#include <llvm/IR/Operator.h>
#include <llvm/Support/raw_ostream.h>
#include <set>

bool callsTransitively(const Function &Caller,
                       const Function &Callee,
                       std::set<const Function *> &Visited) {
    for (auto &BB : Caller) {
        for (auto &Inst : BB) {
            if (auto Call = dyn_cast<CallInst>(&Inst)) {
                auto *Called = getCalledFunction(Call);
                if (Visited.find(Called) != Visited.end())
                    continue;

                Visited.insert(Called);

                if (Called == &Callee)
                    return true;
                if (Called && callsTransitively(*Called, Callee, Visited))
                    return true;
            }
        }
    }
    return false;
}

/// Check if a function can transitively call another function.
/// Since there might be recursion in function calls, use a set of visited
/// functions.
bool callsTransitively(const Function &Caller, const Function &Callee) {
    std::set<const Function *> visited;
    return callsTransitively(Caller, Callee, visited);
}

/// Get function called by a Call instruction. Handles situation when the called
/// value is a bitcast of an actual function call.
const Function *getCalledFunction(const CallInst *Call) {
    const Function *fun = Call->getCalledFunction();
    if (!fun) {
        const Value *val = Call->getCalledValue();
        if (auto BitCast = dyn_cast<BitCastOperator>(val)) {
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