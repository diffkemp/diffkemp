//===----  ReduceFunctionMetadataPass.h - Removes some function metadata --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the ReduceFunctionMetadataPass pass.
///
//===----------------------------------------------------------------------===//

#include "ReduceFunctionMetadataPass.h"

/// Remove custom sections from functions (used when comparing the control flow
/// only)
PreservedAnalyses ReduceFunctionMetadataPass::run(
    Function &Fun, FunctionAnalysisManager &fam) {
    // If the function has a custom section, remove it.
    if (Fun.hasSection())
        Fun.setSection("");

    return PreservedAnalyses();
}
