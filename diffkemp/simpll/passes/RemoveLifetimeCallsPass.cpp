//===------ RemoveLifetimeCallPass.cpp - Removing llvm.lifetime calls -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the RemoveLifetimeCallPass pass.
///
//===----------------------------------------------------------------------===//

#include "RemoveLifetimeCallsPass.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

PreservedAnalyses
        RemoveLifetimeCallsPass::run(Module &Mod,
                                     ModuleAnalysisManager & /*mam*/) {
    std::vector<Instruction *> toRemove;
    for (auto &Fun : Mod) {
        for (auto &BB : Fun) {
            for (auto &Instr : BB) {
                if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                    auto fun = CallInstr->getCalledFunction();
                    if (!fun)
                        continue;
                    auto name = fun->getName();
                    // TODO: this should work with Instr.getIntrinsicID()
                    if (name.find("llvm.lifetime.start") != std::string::npos
                        || name.find("llvm.lifetime.end")
                                   != std::string::npos) {
                        toRemove.push_back(&Instr);
                    }
                }
            }
        }
    }
    for (auto Instr : toRemove)
        Instr->eraseFromParent();
    return PreservedAnalyses();
}
