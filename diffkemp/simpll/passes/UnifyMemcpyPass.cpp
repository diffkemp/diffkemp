//===------ UnifyMemcpyPass.h - Removing arguments of print functions -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the UnifyMemcpyPass.
///
//===----------------------------------------------------------------------===//

#include "UnifyMemcpyPass.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <Config.h>

PreservedAnalyses UnifyMemcpyPass::run(Function &Fun,
                                       FunctionAnalysisManager &fam) {
    std::vector<Instruction *> toRemove;
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto Call = dyn_cast<CallInst>(&Instr)) {
                auto CalledFun = Call->getCalledFunction();
                if (!CalledFun)
                    continue;

                if (CalledFun->getName() == "__memcpy") {
                    DEBUG_WITH_TYPE(DEBUG_SIMPLL, Call->print(dbgs()));
                    // Replace call to __memcpy by llvm.memcpy intrinsic
                    IRBuilder<> builder(&Instr);
                    builder.CreateMemCpy(Call->getArgOperand(0), 0,
                                         Call->getArgOperand(1), 0,
                                         Call->getArgOperand(2));
                    // __memcpy returns pointer to the destination
                    Call->replaceAllUsesWith(Call->getArgOperand(1));
                    toRemove.push_back(Call);
                } else if (CalledFun->getIntrinsicID() == Intrinsic::memcpy) {
                    // If the alignment parameter is set to 1, set it to 0
                    // (LLVM defines 0 and 1 as no alignment)
                    if (auto MemcpyAlign =
                            dyn_cast<ConstantInt>(Call->getArgOperand(3))) {
                        if (MemcpyAlign->getZExtValue() == 1)
                            Call->setArgOperand(
                                    3, ConstantInt::get(MemcpyAlign->getType(),
                                                        0, false));
                    }
                }
            }
        }
    }
    for (auto I : toRemove) {
        I->eraseFromParent();
    }
    return PreservedAnalyses();
}
