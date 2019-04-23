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
#include "Utils.h"
#include <llvm/IR/Module.h>
#include <set>

using namespace llvm;

struct MacroDifference {
	// Name of the macro.
	std::string macroName;
	// The difference.
	std::string BodyL, BodyR;
	// Stacks containing the differing macros and all other macros affected
	// by the difference (again for both modules).
	CallStack StackL, StackR;
	// The function in which the difference was found
	std::string function;
};

class ModuleComparator {
    Module &First;
    Module &Second;
    bool controlFlowOnly;

  public:
    /// Possible results of syntactical function comparison.
    enum Result { EQUAL, NOT_EQUAL, UNKNOWN };
    /// Storing results of function comparisons.
    std::map<FunPair, Result> ComparedFuns;
    /// Storing results from macro comparisons.
    std::vector<MacroDifference> DifferingMacros;

    std::vector<ConstFunPair> MissingDefs;

    /// DebugInfo class storing results from analysing debug information
    const DebugInfo *DI;

    ModuleComparator(Module &First, Module &Second, bool controlFlowOnly,
                     const DebugInfo *DI)
            : First(First), Second(Second), controlFlowOnly(controlFlowOnly),
            GS(&First, &Second, this), DI(DI) {}

    /// Syntactically compare two functions.
    /// The result of the comparison is stored into the ComparedFuns map.
    void compareFunctions(Function *FirstFun, Function *SecondFun);

    /// Pointer to a function that is called just by one of the compared
    /// functions and needs to be inlined.
    std::pair<const CallInst *, const CallInst*> tryInline = {nullptr, nullptr};

  private:
    DifferentialGlobalNumberState GS;
};

#endif //DIFFKEMP_SIMPLL_MODULECOMPARATOR_H
