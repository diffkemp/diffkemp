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
bool PatternComparator::matchPattern(const Instruction *InstL,
                                     const Instruction *InstR) {
    for (auto &&PatFunComparatorPair : PatFunComparators) {
        auto PatComparators = &PatFunComparatorPair.second;
        PatComparators->first->setStartInstruction(InstR);  // New side
        PatComparators->second->setStartInstruction(InstL); // Old side

        // Compare the modules with patterns based on the given module
        // instruction pair.
        if (PatComparators->first->compare() == 0
            && PatComparators->second->compare() == 0) {
            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                            dbgs() << getDebugIndent()
                                   << "Found a match for pattern "
                                   << PatFunComparatorPair.first->Name << "\n");

            // Create a new instruction mapping since the match is valid.
            InstMappings.clear();
            processPatternMatch(PatFunComparatorPair.first, PatComparators);
            return true;
        }
    }

    return false;
}

/// Create the resulting instruction mapping and add all matched
/// instructions into the combined instruction set.
void PatternComparator::processPatternMatch(
        const Pattern *Pat,
        const PatternFunctionComparatorPair *PatComparators) {
    for (auto &&InstPair : PatComparators->first->InstMatchMap) {
        // Add the matched instruction into the set of matched instructions.
        AllInstMatches.insert(InstPair.second);

        // If the instruction is mapped, create the mapping as well.
        if (Pat->FinalMapping.find(InstPair.first) != Pat->FinalMapping.end()) {
            auto MappedPatternInst = Pat->FinalMapping[InstPair.first];
            auto MappedModuleInst =
                    PatComparators->second->InstMatchMap[MappedPatternInst];
            InstMappings[MappedModuleInst] = InstPair.second;
        }
    }

    // Process the matched instructions from the second pattern side.
    for (auto &&InstPair : PatComparators->second->InstMatchMap) {
        AllInstMatches.insert(InstPair.second);
    }
}