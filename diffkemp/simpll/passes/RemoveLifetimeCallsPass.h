//===------- RemoveLifetimeCallPass.h - Removing llvm.lifetime calls ------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the RemoveLifetimeCallPass pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_REMOVELIFETIMECALLSPASS_H
#define DIFFKEMP_SIMPLL_REMOVELIFETIMECALLSPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

/// Pass for removing llvm.lifetime.start and llvm.lifetime.end functions
class RemoveLifetimeCallsPass
        : public PassInfoMixin<RemoveLifetimeCallsPass> {
  public:
    PreservedAnalyses run(Module &Mod, ModuleAnalysisManager &mam);
};

#endif // DIFFKEMP_SIMPLL_REMOVELIFETIMECALLSPASS_H