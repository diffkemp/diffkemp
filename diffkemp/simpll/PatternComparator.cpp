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
#include "Config.h"
#include "Utils.h"

PatternComparator::PatternComparator(const PatternSet *Patterns,
                                     const Function *NewFun,
                                     const Function *OldFun) {
    // Populate the pattern function comparator map.
    for (auto &Pattern : *Patterns) {
        auto NewPatFunComparator = std::make_unique<PatternFunctionComparator>(
                NewFun, Pattern.NewPattern, &Pattern);
        auto OldPatFunComparator = std::make_unique<PatternFunctionComparator>(
                OldFun, Pattern.OldPattern, &Pattern);

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
        auto PatComparators = &PatFunComparatorPair.second;
        PatComparators->first->setStartInstruction(NewInst);
        PatComparators->second->setStartInstruction(OldInst);

        // Compare the modules with patterns based on the given module
        // instruction pair.
        if (PatComparators->first->compare() == 0
            && PatComparators->second->compare() == 0) {
            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                            dbgs() << getDebugIndent()
                                   << "Found a match for pattern "
                                   << PatFunComparatorPair.first->Name << "\n");

            // Save the result since the match is valid.
            PatComparators->first->saveResult(&InstMatches);
            PatComparators->second->saveResult(&InstMatches);

            // TODO: Should return an elaborate comparison result map
            //       with matched instructions which may be ignored.
            return true;
        }
    }

    return false;
}
