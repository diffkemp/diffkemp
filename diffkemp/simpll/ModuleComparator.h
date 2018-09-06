//===------------ ModuleComparator.h - Numbering global symbols -----------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the ModuleComparator class that can be
/// used for syntactical comparison of two LLVM modules.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_MODULECOMPARATOR_H
#define DIFFKEMP_SIMPLL_MODULECOMPARATOR_H

#include "DebugInfo.h"
#include "DifferentialGlobalNumberState.h"
#include <llvm/IR/Module.h>
#include <set>

using namespace llvm;

class ModuleComparator {
    Module &First;
    Module &Second;

  public:
    /// Possible results of syntactical function comparison.
    enum Result { EQUAL, NOT_EQUAL, UNKNOWN };
    /// Storing results of function comparisons.
    using FunPair = std::pair<Function *, Function *>;
    std::map<FunPair, Result> ComparedFuns;

    /// DebugInfo class storing results from analysing debug information
    const DebugInfo *DI;

    ModuleComparator(Module &First, Module &Second,
                     const DebugInfo *DI)
            : First(First), Second(Second), GS(&First, &Second, this),
            DI(DI) {}

    /// Syntactically compare two functions.
    /// The result of the comparison is stored into the ComparedFuns map.
    void compareFunctions(Function *FirstFun, Function *SecondFun);

  private:
    DifferentialGlobalNumberState GS;
};

#endif //DIFFKEMP_SIMPLL_MODULECOMPARATOR_H
