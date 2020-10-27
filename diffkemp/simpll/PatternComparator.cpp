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
/// comparator, which enables eliminations of reports of known differences.
///
//===----------------------------------------------------------------------===//

#include "PatternComparator.h"

PatternComparator::PatternComparator(const PatternSet *Patterns,
                                     const Function *NewFun,
                                     const Function *OldFun) {
    // Populate the active pattern and pattern function comparator maps.
    for (auto &Pattern : *Patterns) {
        ActivePatterns.emplace(&Pattern, std::vector<ActivePattern>{});

        auto NewPatFunComparator = std::make_unique<PatternFunctionComparator>(
                NewFun, Pattern.NewPattern);
        auto OldPatFunComparator = std::make_unique<PatternFunctionComparator>(
                OldFun, Pattern.OldPattern);
        PatFunComparators.emplace(
                &Pattern,
                std::make_pair(std::move(NewPatFunComparator),
                               std::move(OldPatFunComparator)));
    }
};

/// Tries to match the given instruction pair to the starting instructions
/// of one of the patterns. Returns true if a valid match is found.
bool PatternComparator::matchPatternStart(const Instruction *NewInst,
                                          const Instruction *OldInst) {
    bool PatternMatched = false;

    for (auto &&ActPatternPair : ActivePatterns) {
        // Retrieve the corresponding pattern function comparators.
        auto PatFunComparatorPair = &PatFunComparators[ActPatternPair.first];

        // Compare the given module instructions with both starting pattern
        // instructions.
        if (!PatFunComparatorPair->first->cmpOperationsWithOperands(
                    NewInst, ActPatternPair.first->NewStartPosition)
            && !PatFunComparatorPair->second->cmpOperationsWithOperands(
                    OldInst, ActPatternPair.first->OldStartPosition)) {
            PatternMatched = true;

            // Generate the corresponding active pattern instance.
            ActPatternPair.second.emplace_back(ActPatternPair.first);
        }
    }

    return PatternMatched;
}

/// Tries to match the given instruction pair to one of the active patterns.
/// Returns true if a valid match is found.
bool PatternComparator::matchActivePattern(const Instruction *NewInst,
                                           const Instruction *OldInst) {
    bool PatternMatched = false;

    for (auto &&ActPatternPair : ActivePatterns) {
        // Retrieve the corresponding pattern function comparators.
        auto PatFunComparatorPair = &PatFunComparators[ActPatternPair.first];

        // Compare the given module instructions with the current positions of
        // active patterns.
        for (auto &&ActPattern : ActPatternPair.second) {
            if (!PatFunComparatorPair->first->cmpOperationsWithOperands(
                        NewInst, ActPattern.NewPosition)
                && !PatFunComparatorPair->second->cmpOperationsWithOperands(
                        OldInst, ActPattern.OldPosition)) {
                PatternMatched = true;
                ++ActPattern.NewPosition;
                ++ActPattern.OldPosition;

                // TODO: Check pattern completeness.
            }
        }
    }

    return PatternMatched;
}
