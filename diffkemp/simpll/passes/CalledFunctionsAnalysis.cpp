//===----- CalledFunctionsAnalysis.h - Abstracting non-function calls -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the CalledFunctionsAnalysis pass
/// that collects all functions potentially called by the main function.
///
//===----------------------------------------------------------------------===//

#include "CalledFunctionsAnalysis.h"
#include "Utils.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/User.h>

AnalysisKey CalledFunctionsAnalysis::Key;

CalledFunctionsAnalysis::Result CalledFunctionsAnalysis::run(
        Module &Mod, AnalysisManager<Module, Function *> &mam, Function *Main) {
    Result result;
    collectCalled(Main, result);
    return result;
}

/// Recursively collect all functions potentially called by Fun and add them to
/// the Called set. All functions called by 'call' instructions and used as
/// operands to some instructions in Fun are collected.
void CalledFunctionsAnalysis::collectCalled(const Function *Fun,
                                            Result &Called) {
    if (Called.find(Fun) != Called.end())
        return;

    Called.insert(Fun);

    for (auto &BB : *Fun) {
        for (auto &Inst : BB) {
            if (auto Call = dyn_cast<CallInst>(&Inst)) {
                if (auto *CalledFun =
                            getCalledFunction(Call->getCalledValue())) {
                    collectCalled(CalledFun, Called);
                }
            }
            for (auto &Op : Inst.operands()) {
                processValue(Op, Called);
            }
        }
    }
}

/// Looks for functions in a value (either a function itself, or a composite
/// type constant).
void CalledFunctionsAnalysis::processValue(const Value *Val, Result &Called) {
    if (auto Fun = getCalledFunction(Val))
        collectCalled(Fun, Called);
    else if (auto GV = dyn_cast<GlobalVariable>(Val)) {
        if (GV->hasInitializer() && GV->isConstant())
            // The initializer is constant - see whether it contains
            // a function (or a user type constant that contains
            // a function).
            if (auto U = dyn_cast<User>(GV->getInitializer())) {
                for (auto &UserOp : U->operands()) {
                    processValue(UserOp.get(), Called);
                }
            } else {
                processValue(GV->getInitializer(), Called);
            }
    } else if (auto BitCast = dyn_cast<BitCastOperator>(Val)) {
        processValue(BitCast->getOperand(0), Called);
    }
}
