//===------------- PatternComparator.h - Code pattern finder --------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the LLVM code pattern finder and
/// comparison manager, which enables eliminations of reports of known
/// module differences.
///
//===----------------------------------------------------------------------===//

#include "PatternComparator.h"
#include "DebugInfo.h"

PatternComparator::PatternComparator(const PatternSet *Patterns,
                                     const Function *NewFun,
                                     const Function *OldFun) {
    // Populate the pattern function comparator map.
    for (auto &Pattern : *Patterns) {
        auto NewPatFunComparator = std::make_unique<PatternFunctionComparator>(
                NewFun, Pattern.NewPattern, Patterns);
        auto OldPatFunComparator = std::make_unique<PatternFunctionComparator>(
                OldFun, Pattern.OldPattern, Patterns);

        PatFunComparators.emplace(
                &Pattern,
                std::make_pair(std::move(NewPatFunComparator),
                               std::move(OldPatFunComparator)));
    }
};

/// Tries to match a difference pattern starting with instructions that may
/// be matched to the given instruction pair. Returns true if a valid match
/// is found.
bool PatternComparator::matchPattern(const Instruction *NewInst,
                                     const Instruction *OldInst) {
    for (auto &&PatFunComparatorPair : PatFunComparators) {
        // Compare the modules with patterns based on the given module
        // instruction pair.
        auto PatComparators = &PatFunComparatorPair.second;
        if (!PatComparators->first->compareFromInst(NewInst)
            && !PatComparators->second->compareFromInst(OldInst)) {
            // TODO: Should return an elaborate comparison result map
            //       with matched instructions which may be ignored.
            return true;
        }
    }

    return false;
}
