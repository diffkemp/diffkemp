//===-- ReduceFunctionMetadataPass.cpp - Removes some function metadata ---===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the ReduceFunctionMetadataPass pass.
/// The pass currently removes custom sections from functions.
///
//===----------------------------------------------------------------------===//

#include "ReduceFunctionMetadataPass.h"
#include <llvm/IR/Instructions.h>

/// Remove custom sections from functions (used when comparing the control flow
/// only)
PreservedAnalyses
        ReduceFunctionMetadataPass::run(Function &Fun,
                                        FunctionAnalysisManager & /*fam*/) {
    // If the function has a custom section, remove it.
    if (Fun.hasSection())
        Fun.setSection("");

    // Distinguishing linkage type is pointless, because it would only cause
    // function inlining, discarding the attribute.
    Fun.setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

    // Removing tail signifier from function calls
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto Call = dyn_cast<CallInst>(&Instr)) {
                Call->setTailCall(false);
            }
        }
    }

    return PreservedAnalyses::all();
}
