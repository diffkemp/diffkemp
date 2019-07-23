//===------------- ModuleComparator.h - Comparing LLVM modules ------------===//
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
#include "SourceCodeUtils.h"
#include "passes/StructureSizeAnalysis.h"
#include "Utils.h"
#include <llvm/IR/Module.h>
#include <set>

using namespace llvm;

class ModuleComparator {
    Module &First;
    Module &Second;
    bool controlFlowOnly;

  public:
    /// Possible results of syntactical function comparison.
    enum Result { EQUAL, NOT_EQUAL, UNKNOWN };
    /// Storing results of function comparisons.
    std::map<FunPair, Result> ComparedFuns;
    /// Storing results from macro and asm comparisions.
    std::vector<SyntaxDifference> DifferingObjects;
    // Function abstraction to assembly string map.
    StringMap<StringRef> AsmToStringMapL;
    StringMap<StringRef> AsmToStringMapR;
    // Structure size to structure name map.
    StructureSizeAnalysis::Result &StructSizeMapL;
    StructureSizeAnalysis::Result &StructSizeMapR;
    // Counter of assembly diffs
    int asmDifferenceCounter = 0;

    std::vector<ConstFunPair> MissingDefs;

    /// DebugInfo class storing results from analysing debug information
    const DebugInfo *DI;

    ModuleComparator(Module &First, Module &Second, bool controlFlowOnly,
                     const DebugInfo *DI, StringMap<StringRef> &AsmToStringMapL,
                     StringMap<StringRef> &AsmToStringMapR,
                     StructureSizeAnalysis::Result &StructSizeMapL,
                     StructureSizeAnalysis::Result &StructSizeMapR)
            : First(First), Second(Second), controlFlowOnly(controlFlowOnly),
            GS(&First, &Second, this), DI(DI), AsmToStringMapL(AsmToStringMapL),
            AsmToStringMapR(AsmToStringMapR), StructSizeMapL(StructSizeMapL),
            StructSizeMapR(StructSizeMapR) {}

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
