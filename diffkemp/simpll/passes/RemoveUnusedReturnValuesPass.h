//===--- RemoveUnusedReturnValuesPass.h - Transforming functions to void --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the RemoveUnusedReturnValues pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_REMOVEUNUSEDRETURNVALUESPASS_H
#define DIFFKEMP_SIMPLL_REMOVEUNUSEDRETURNVALUESPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

/// A pass that transforms functions returning some value to void in case their
/// return value is never used.
class RemoveUnusedReturnValuesPass
        : public PassInfoMixin<RemoveUnusedReturnValuesPass> {
  public:
    PreservedAnalyses run(Module &Mod, AnalysisManager<Module, Function *> &mam,
                          Function *Main);
};

#endif //DIFFKEMP_SIMPLL_REMOVEUNUSEDRETURNVALUESPASS_H