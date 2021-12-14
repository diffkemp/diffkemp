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
#include <Config.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>

PreservedAnalyses UnifyMemcpyPass::run(Function &Fun,
                                       FunctionAnalysisManager & /*fam*/) {
    std::vector<Instruction *> toRemove;
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto Call = dyn_cast<CallInst>(&Instr)) {
                auto CalledFun = Call->getCalledFunction();
                if (!CalledFun)
                    continue;

                if (CalledFun->getName() == "__memcpy") {
                    // Replace call to __memcpy by llvm.memcpy intrinsic
                    IRBuilder<> builder(&Instr);
#if LLVM_VERSION_MAJOR < 7
                    builder.CreateMemCpy(Call->getArgOperand(0),
                                         Call->getArgOperand(1),
                                         Call->getArgOperand(2),
                                         0);
#elif LLVM_VERSION_MAJOR < 10
                    builder.CreateMemCpy(Call->getArgOperand(0),
                                         0,
                                         Call->getArgOperand(1),
                                         0,
                                         Call->getArgOperand(2));
#else
                    builder.CreateMemCpy(Call->getArgOperand(0),
                                         MaybeAlign(0),
                                         Call->getArgOperand(1),
                                         MaybeAlign(0),
                                         Call->getArgOperand(2));
#endif
                    // __memcpy returns pointer to the destination
                    Call->replaceAllUsesWith(Call->getArgOperand(1));
                    toRemove.push_back(Call);
                } else if (CalledFun->getIntrinsicID() == Intrinsic::memcpy) {
                    // If the alignment parameter is set to 0, set it to 1
                    // (LLVM defines 0 and 1 as no alignment).
                    // Note: in LLVM 7 and higher, alignment is represented by
                    // an attribute instead of an argument. The value is set to
                    // 1 instead of 0 because of a bug in LLVM 7 to 9 which
                    // breaks setting the alignment to zero.
#if LLVM_VERSION_MAJOR < 7
                    if (auto MemcpyAlign =
                                dyn_cast<ConstantInt>(Call->getArgOperand(3))) {
                        if (MemcpyAlign->getZExtValue() == 0)
                            Call->setArgOperand(
                                    3,
                                    ConstantInt::get(
                                            MemcpyAlign->getType(), 1, false));
                    }
#elif LLVM_VERSION_MAJOR < 10
                    for (int i : {0, 1}) {
                        if (Call->getParamAlignment(i) == 0) {
                            Call->removeParamAttr(i, Attribute::Alignment);
                            Call->addParamAttr(i,
                                               Attribute::getWithAlignment(
                                                       Call->getContext(), 1));
                        }
                    }
#else
                    for (int i : {0, 1}) {
                        if (Call->getParamAlign(i) == 0) {
                            Call->removeParamAttr(i, Attribute::Alignment);
                            Call->addParamAttr(
                                    i,
                                    Attribute::getWithAlignment(
                                            Call->getContext(), Align(1)));
                        }
                    }
#endif
                }
            }
        }
    }
    for (auto I : toRemove) {
        I->eraseFromParent();
    }
    return PreservedAnalyses();
}
