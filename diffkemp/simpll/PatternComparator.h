//===------------- PatternComparator.h - Code pattern finder --------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the LLVM code pattern finder and
/// comparison manager. The comparator supports both instruction-based and
/// value-based difference patterns.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
#define DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H

#include "InstPatternComparator.h"
#include "PatternSet.h"
#include "ValuePatternComparator.h"
#include <llvm/IR/Instructions.h>
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Compares difference patterns against functions, possibly eliminating reports
/// of prior semantic differences.
/// Supported difference patterns:
///   - Instruction patterns - Patterns consisting of standard LLVM IR code,
///     i.e., typically containing multiple instructions and basic blocks.
///     Instructions may be annotated using diffkemp.pattern metadata.
///   - Value patterns - Patterns describing a change of a single value. The
///     original and the modified value are described strictly through return
///     instructions.
/// Concrete examples of difference patterns can be seen in regression tests.
class PatternComparator {
  public:
    PatternComparator(const PatternSet *Patterns,
                      const DifferentialFunctionComparator *DiffFunctionComp,
                      const Function *FnL,
                      const Function *FnR)
            : DiffFunctionComp(DiffFunctionComp) {
        // Populate both pattern function comparator maps.
        for (auto &InstPattern : Patterns->InstPatterns) {
            auto PatternFunCompL = std::make_unique<InstPatternComparator>(
                    FnL, InstPattern.PatternL, &InstPattern);
            auto PatternFunCompR = std::make_unique<InstPatternComparator>(
                    FnR, InstPattern.PatternR, &InstPattern);

            InstPatternComps.try_emplace(
                    &InstPattern,
                    std::make_pair(std::move(PatternFunCompL),
                                   std::move(PatternFunCompR)));
        }
        for (auto &ValuePattern : Patterns->ValuePatterns) {
            auto PatternFunCompL = std::make_unique<ValuePatternComparator>(
                    FnL, ValuePattern.PatternL, &ValuePattern);
            auto PatternFunCompR = std::make_unique<ValuePatternComparator>(
                    FnR, ValuePattern.PatternR, &ValuePattern);

            ValuePatternComps.try_emplace(
                    &ValuePattern,
                    std::make_pair(std::move(PatternFunCompL),
                                   std::move(PatternFunCompR)));
        }
    };

    /// Individual matched instructions, combined for all matched patterns.
    /// Combines results of all active pattern matches.
    InstructionSet AllInstMatches;
    /// Mapping for matched module instructions. Instructions from the left
    /// module are used as keys, while instructions from the right module
    /// are used as values. Contains the result of a single pattern match.
    mutable InstructionMap InstMappings;

    /// Tries to match a difference pattern starting with the given instruction
    /// pair. Returns true if a valid match is found.
    bool matchPattern(const Instruction *InstL, const Instruction *InstR);

    /// Tries to match a pair of values to a value pattern. Returns true if a
    /// valid match is found.
    bool matchValues(const Value *L, const Value *R);

  private:
    /// Pair of corresponding instruction pattern function comparators.
    using InstPatternComparatorPair =
            std::pair<std::unique_ptr<InstPatternComparator>,
                      std::unique_ptr<InstPatternComparator>>;
    /// Pair of corresponding value pattern function comparators.
    using ValuePatternComparatorPair =
            std::pair<std::unique_ptr<ValuePatternComparator>,
                      std::unique_ptr<ValuePatternComparator>>;

    /// Parent DifferentialFunctionComparator the compares the module functions.
    const DifferentialFunctionComparator *DiffFunctionComp;
    /// Instruction pattern function comparators associated with the current set
    /// of patterns and the currently compared functions.
    DenseMap<const InstPattern *, InstPatternComparatorPair> InstPatternComps;
    /// Value pattern function comparators associated with the current set of
    /// patterns and the currently compared functions.
    DenseMap<const ValuePattern *, ValuePatternComparatorPair>
            ValuePatternComps;

    /// Tries to match one of the loaded instruction patterns. Returns true
    /// if a valid match is found.
    bool matchInstPattern(const Instruction *InstL, const Instruction *InstR);

    /// Tries to match one of the loaded value patterns. Returns true if
    /// a valid match is found.
    bool matchValuePattern(const Instruction *InstL, const Instruction *InstR);

    /// Tries to match a load instruction to the start of the given value
    /// pattern.
    bool matchLoadInst(const LoadInst *Load,
                       const ValuePattern *Pat,
                       bool IsLeftSide);

    /// Check whether the input, value, and type mapping generated by the given
    /// pattern function comparator pair is valid even when both compared
    /// modules are analysed at once.
    bool pairwiseMappingValid(const InstPattern *Pat,
                              const InstPatternComparatorPair *PatternComps);

    /// Create the resulting instruction mapping and add all matched
    /// instructions into the combined instruction set.
    void processPatternMatch(const InstPattern *Pat,
                             const InstPatternComparatorPair *PatternComps);
};

#endif // DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
