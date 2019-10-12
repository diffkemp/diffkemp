//== MergeNumberedFunctionsPass.h - Merge functions with different numbers ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the MergeNumberedFunctionsPass pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_MERGENUMBEREDFUNCTIONSPASS_H
#define DIFFKEMP_SIMPLL_MERGENUMBEREDFUNCTIONSPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

/// Merges functions with names differing only in number suffixes if they are
/// equal.
class MergeNumberedFunctionsPass
        : public PassInfoMixin<MergeNumberedFunctionsPass> {
  public:
    PreservedAnalyses run(Module &Mod, AnalysisManager<Module> &mam);
};

#endif // DIFFKEMP_SIMPLL_MERGENUMBEREDFUNCTIONSPASS_H
