//===- PrintContentRemovalPass.cpp - Removing arguments of print functions ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the PrintContentRemovalPass that
/// removes arguments of all printing functions.
///
//===----------------------------------------------------------------------===//

#include "PrintContentRemovalPass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

PreservedAnalyses PrintContentRemovalPass::run(
        Function &Fun,
        FunctionAnalysisManager &fam) {
    std::vector<Instruction *> toRemove;
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                auto CalledFun = CallInstr->getCalledFunction();

                if (CalledFun && CalledFun->getName() == "printk") {
                    // Functions with 1 mandatory argument
                    auto OpType = dyn_cast<PointerType>(
                            CallInstr->getOperand(0)->getType());
                    auto newCall = CallInst::Create(
                            CalledFun,
                            {ConstantPointerNull::get(OpType)},
                            "", &Instr);
                    CallInstr->replaceAllUsesWith(newCall);
                    toRemove.push_back(&Instr);
                } else if (CalledFun && (CalledFun->getName() == "_dev_info" ||
                           CalledFun->getName() == "dev_warn" ||
                           CalledFun->getName() == "dev_err" ||
                           CalledFun->getName() == "sprintf")) {
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
                    CallInstr->replaceAllUsesWith(newCall);
                    toRemove.push_back(&Instr);
                }
            }
        }
    }
    for (auto i : toRemove)
        i->eraseFromParent();
    toRemove.clear();

    return PreservedAnalyses();
}