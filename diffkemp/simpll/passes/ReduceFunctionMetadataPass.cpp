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

/// Remove custom sections from functions (used when comparing the control flow
/// only)
PreservedAnalyses
        ReduceFunctionMetadataPass::run(Function &Fun,
                                        FunctionAnalysisManager &fam) {
    // If the function has a custom section, remove it.
    if (Fun.hasSection())
        Fun.setSection("");

    // Distinguishing linkage type is pointless, because it would only cause
    // function inlining, discarding the attribute.
    Fun.setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

    return PreservedAnalyses::all();
}
