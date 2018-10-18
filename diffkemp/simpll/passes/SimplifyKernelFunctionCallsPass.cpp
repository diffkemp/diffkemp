// SimplifyKernelFunctionCallsPass.cpp - Simplifying kernel-specific functions//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the SimplifyKernelFunctionCallsPass.
/// Applied transformations:
/// 1. Remove all arguments of calls to printing functions (printk, _dev_info,
///    dev_warn, dev_err, sprintf).
/// 2. Remove the second argument of all calls to warn_slowpath_null. This
///    argument is a line number.
/// 3. Remove the second argument of inline assemblies containing the
///    __bug_table string. The argument is a line number.
///
//===----------------------------------------------------------------------===//

#include "SimplifyKernelFunctionCallsPass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>

/// Replace the second argument of a call instruction by 0.
/// Checks if the argument is of integer type.
void replaceSecondArgByZero(CallInst *Call) {
    auto OldArg = dyn_cast<ConstantInt>(Call->getArgOperand(1));
    if (OldArg->getType()->isIntegerTy()) {
        Call->setArgOperand(1, ConstantInt::get(OldArg->getType(),
                                                APInt(OldArg->getBitWidth(),
                                                      0)));
    }
}

PreservedAnalyses SimplifyKernelFunctionCallsPass::run(
        Function &Fun,
        FunctionAnalysisManager &fam) {
    std::vector<Instruction *> toRemove;
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                auto CalledFun = CallInstr->getCalledFunction();

                if (!CalledFun) {
                    // Replace the second argument of inline asm containing
                    // __bug_table by 0.
                    auto CalledVal = CallInstr->getCalledValue();
                    if (auto Asm = dyn_cast<InlineAsm>(CalledVal)) {
                        if (Asm->getAsmString().find("__bug_table")
                                != std::string::npos)
                            replaceSecondArgByZero(CallInstr);
                    }
                    continue;
                }

                // Remove arguments of printing functions
                if (CalledFun->getName() == "printk") {
                    // Functions with 1 mandatory argument
                    auto OpType = dyn_cast<PointerType>(
                            CallInstr->getOperand(0)->getType());
                    // An additional void pointer is added to the operand list
                    // so the instruction can be compared as equal even when the
                    // other one is one of the functions listed in the other if
                    auto newCall = CallInst::Create(
                            CalledFun,
                            {ConstantPointerNull::get(OpType),
                             ConstantPointerNull::get(OpType)},
                            "", &Instr);
                    newCall->setDebugLoc(CallInstr->getDebugLoc());
                    CallInstr->replaceAllUsesWith(newCall);
                    toRemove.push_back(&Instr);
                } else if (CalledFun->getName() == "_dev_info" ||
                           CalledFun->getName() == "dev_warn" ||
                           CalledFun->getName() == "dev_err" ||
                           CalledFun->getName() == "sprintf") {
                    // Functions with 2 mandatory arguments
                    auto Op0Type = dyn_cast<PointerType>(
                            CallInstr->getOperand(0)->getType());
                    auto Op1Type = dyn_cast<PointerType>(
                            CallInstr->getOperand(1)->getType());
                    auto newCall = CallInst::Create(
                            CalledFun,
                            {ConstantPointerNull::get(Op0Type),
                             ConstantPointerNull::get(Op1Type)},
                            "", &Instr);
                    newCall->setDebugLoc(CallInstr->getDebugLoc());
                    CallInstr->replaceAllUsesWith(newCall);
                    toRemove.push_back(&Instr);
                }

                // Replace the second argument of a call to warn_slowpath_null
                // by 0.
                if (CalledFun->getName() == "warn_slowpath_null" ||
                        CalledFun->getName() == "warn_slowpath_fmt" ||
                        CalledFun->getName() == "__might_sleep") {
                    replaceSecondArgByZero(CallInstr);
                }

            }
        }
    }
    for (auto i : toRemove)
        i->eraseFromParent();
    toRemove.clear();

    return PreservedAnalyses();
}