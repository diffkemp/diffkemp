//===------ FixParameterPass.h - Replacing function parameters -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the FixParameterPass that replaces
// all uses of a specified function parameter with a fixed constant integer
// value throughout the function body.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_FIXPARAMETERPASS_H
#define DIFFKEMP_SIMPLL_FIXPARAMETERPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

class FixParameterPass : public PassInfoMixin<FixParameterPass> {
  private:
    int ParamIndex;
    uint64_t FixedValue;

  public:
    FixParameterPass(unsigned paramIndex, uint64_t fixedValue)
            : ParamIndex(paramIndex), FixedValue(fixedValue) {}

    PreservedAnalyses run(Function &Fun, FunctionAnalysisManager &fam);
};

#endif // DIFFKEMP_SIMPLL_FIXPARAMETERPASS_H
