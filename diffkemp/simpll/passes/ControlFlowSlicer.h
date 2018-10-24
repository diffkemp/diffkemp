//=====- ControlFlowSlicer.h - Slicing out non-control-flow dependencies-=====//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the ControlFlowSlicer pass.
/// The pass slices the program to keep only those instructions that affect
/// the control-flow (branching, loops, functions calls, jumps).
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_CONTROLFLOWSLICER_H
#define DIFFKEMP_SIMPLL_CONTROLFLOWSLICER_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

class ControlFlowSlicer : public PassInfoMixin<ControlFlowSlicer> {
  public:
    PreservedAnalyses run(Function &Fun, FunctionAnalysisManager &fam);
};

#endif //DIFFKEMP_SIMPLL_CONTROLFLOWSLICER_H
