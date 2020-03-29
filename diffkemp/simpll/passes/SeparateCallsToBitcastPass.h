//===-----  SeparateCallsToBitcastPass.h - Separate calls to bitcasts -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the SeparateCallsToBitcastPass pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_SEPARATECALLSTOBITCASTPASS_H
#define DIFFKEMP_SIMPLL_SEPARATECALLSTOBITCASTPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

/// Separate bitcasts from calls to bitcast operators.
class SeparateCallsToBitcastPass
        : public PassInfoMixin<SeparateCallsToBitcastPass> {
  public:
    PreservedAnalyses run(Function &Fun, FunctionAnalysisManager &fam);
};

#endif // DIFFKEMP_SIMPLL_SEPARATECALLSTOBITCASTPASS_H
