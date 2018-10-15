//==--- SimplifyKernelGlobalsPass.h - Simplifying kernel-specific globals --==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the SimplifyKernelGlobalsPass that
/// simplifies kernel-specific global variables and functions that do not affect
/// semantics of the program.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_SIMPLIFYKERNELGLOBALSPASS_H
#define DIFFKEMP_SIMPLL_SIMPLIFYKERNELGLOBALSPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

class SimplifyKernelGlobalsPass
        : public PassInfoMixin<SimplifyKernelGlobalsPass> {
  public:
    PreservedAnalyses run(Module &Mod, ModuleAnalysisManager &mam);
};

#endif //DIFFKEMP_SIMPLL_SIMPLIFYKERNELGLOBALSPASS_H
