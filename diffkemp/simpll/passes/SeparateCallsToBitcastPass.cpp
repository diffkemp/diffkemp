//===----  SeparateCallsToBitcastPass.cpp - Separate calls to bitcasts ----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the SeparateCallsToBitcastPass pass.
/// The pass separates bitcasts from calls to bitcast operators by inserting
/// new bitcast instructions that transform function arguments and the return
/// value separately.
///
//===----------------------------------------------------------------------===//

#include "SeparateCallsToBitcastPass.h"

#include "Utils.h"
#include <Config.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>

/// Separate bitcasts from calls to bitcast operators to make the calls
/// inlinable.
PreservedAnalyses
        SeparateCallsToBitcastPass::run(Function &Fun,
                                        FunctionAnalysisManager &fam) {
    std::vector<Instruction *> toRemove;

    for (auto &BB : Fun) {
        for (auto &Inst : BB) {
            if (auto Call = dyn_cast<CallInst>(&Inst)) {
                // Find call instructions calling a bitcast
                // operator that directly corresponds to a function.
                auto BitCast =
                        dyn_cast<BitCastOperator>(Call->getCalledValue());

                if (BitCast && isa<Function>(BitCast->getOperand(0))) {
                    // Get the bitcasted function.
                    auto srcFun =
                            dyn_cast<Function>(BitCast->stripPointerCasts());

                    // Ignore the instruction if the number of
                    // arguments is lower than the number of parameters.
                    if (Call->getNumArgOperands()
                        < srcFun->getFunctionType()->getNumParams()) {
                        continue;
                    }

                    // Ignore the instruction if the source function returns
                    // void and the return value is used.
                    if (srcFun->getReturnType()->isVoidTy()
                        && !Call->getType()->isVoidTy()) {
                        continue;
                    }

                    // Bitcast all arguments to the types of source function
                    // parameters. If the number of arguments is higher, use the
                    // remaining arguments without bitcasting as a vargars is
                    // present in the call instruction.
                    std::vector<Value *> newArgs;
                    auto arg = Call->arg_begin();

                    for (auto paramType : srcFun->getFunctionType()->params()) {
                        if ((*arg)->getType() == paramType) {
                            newArgs.push_back(*arg);
                        } else {
                            // Bitcast the argument so that the types match.
                            auto newArg = CastInst::Create(
                                    Instruction::BitCast,
                                    *arg,
                                    paramType,
                                    "",
                                    newArgs.empty() ? cast<Instruction>(Call)
                                                    : cast<Instruction>(
                                                            newArgs.back()));

                            newArg->setDebugLoc(Call->getDebugLoc());
                            newArgs.push_back(newArg);
                        }

                        ++arg;
                    }

                    // Add the remaining arguments if there are any.
                    while (arg != Call->arg_end()) {
                        newArgs.push_back(*arg);
                        ++arg;
                    }

                    // Create a new call instruction using the
                    // source function and bitcasted arguments.
                    auto newCall = CallInst::Create(srcFun, newArgs, "", Call);
                    Instruction *replacementValue = newCall;
                    copyCallInstProperties(Call, newCall);

                    if (Call->getType() != newCall->getType()
                        && !newCall->getType()->isVoidTy()) {
                        // If return types do not match, bitcast the new
                        // call result to the original result type.
                        // Calls with a void return type are not bitcasted.
                        auto returnBitCast =
                                CastInst::Create(Instruction::BitCast,
                                                 newCall,
                                                 Call->getType(),
                                                 "",
                                                 Call);

                        returnBitCast->setDebugLoc(Call->getDebugLoc());
                        replacementValue = returnBitCast;
                    }

                    // Replace the old call instruction with the last generated
                    // instruction.
                    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                    dbgs() << "Replacing :" << *Call
                                           << "\n   with :" << *replacementValue
                                           << "\n");
                    Call->replaceAllUsesWith(replacementValue);
                    toRemove.push_back(Call);
                }
            }
        }
    }

    // Remove replaced call instructions.
    for (auto Call : toRemove) {
        Call->eraseFromParent();
    }

    return PreservedAnalyses();
}
