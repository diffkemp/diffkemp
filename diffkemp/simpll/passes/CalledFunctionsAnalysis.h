//===----- CalledFunctionsAnalysis.h - Abstracting non-function calls -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the CalledFunctionsAnalysis pass that
/// collects all functions potentially called by the main function.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_CALLEDFUNCTIONSANALYSIS_H
#define DIFFKEMP_SIMPLL_CALLEDFUNCTIONSANALYSIS_H

#include <llvm/IR/PassManager.h>
#include <llvm/IR/PassManagerImpl.h>
#include <set>

using namespace llvm;

class CalledFunctionsAnalysis
        : public AnalysisInfoMixin<CalledFunctionsAnalysis> {
  public:
    using Result = std::set<const Function *>;
    Result run(Module &Mod,
               AnalysisManager<Module, Function *> &mam,
               Function *Main);

  protected:
    /// Collect all functions called by Fun.
    /// \param Fun
    /// \param Called Resulting set of collected functions.
    void collectCalled(const Function *Fun, Result &Called);
    /// Looks for functions in a value (either a function itself, or a composite
    /// type constant).
    void processValue(const Value *Val, Result &Called);

  private:
    friend AnalysisInfoMixin<CalledFunctionsAnalysis>;
    static AnalysisKey Key;
    /// The set of values that were already processed in the current run.
    /// Prevents infinite recursion when processing instruction operands.
    std::set<const Value *> ProcessedValues;
};

#endif // DIFFKEMP_SIMPLL_CALLEDFUNCTIONSANALYSIS_H
