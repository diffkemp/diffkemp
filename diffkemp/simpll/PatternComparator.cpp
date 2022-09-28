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
/// module differences. The comparator supports both instruction-based and
/// value-based difference patterns.
///
//===----------------------------------------------------------------------===//

#include "PatternComparator.h"
#include "Config.h"
#include "DifferentialFunctionComparator.h"
#include "Utils.h"

/// Tries to match a difference pattern starting with the given instruction
/// instruction pair. Returns true if a valid match is found. Instruction
/// patterns are prioritized over value patterns. Only a single pattern
/// match is expected to be possible at once.
bool PatternComparator::matchPattern(const Instruction *InstL,
                                     const Instruction *InstR) {
    return matchInstPattern(InstL, InstR) || matchValuePattern(InstL, InstR);
}

/// Tries to match a pair of values to a value pattern. Returns true if a
/// valid match is found.
bool PatternComparator::matchValues(const Value *L, const Value *R) {
    // Try to match the difference to a value-based pattern.
    for (auto &&ValuePatternCompPair : ValuePatternComps) {
        auto PatternComps = &ValuePatternCompPair.second;
        PatternComps->first->ComparedValue = L;
        PatternComps->second->ComparedValue = R;

        // Compare the module values with values from patterns.
        if (PatternComps->first->compare() == 0
            && PatternComps->second->compare() == 0) {
            // If both compared values are load instructions, ensure that they
            // are mapped to each other as well.
            if (isa<LoadInst>(L) && isa<LoadInst>(R)
                && DiffFunctionComp->cmpValuesByMapping(L, R)) {
                continue;
            }

            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                            dbgs() << getDebugIndent()
                                   << "Found a match for value pattern "
                                   << ValuePatternCompPair.first->Name << "\n");
            return true;
        }
    }

    return false;
}

/// Tries to match one of the loaded instruction patterns. Returns true
/// if a valid match is found.
bool PatternComparator::matchInstPattern(const Instruction *InstL,
                                         const Instruction *InstR) {
    // Try to find an instruction-based pattern matching the starting
    // instruction pair.
    for (auto &&InstPatternCompPair : InstPatternComps) {
        auto PatternComps = &InstPatternCompPair.second;
        PatternComps->first->StartInst = InstL;
        PatternComps->second->StartInst = InstR;

        // To eliminate the difference, at least one of the given instructions
        // needs to be a part of the pattern.
        if (PatternComps->first->compareStartInst() != 0
            && PatternComps->second->compareStartInst() != 0) {
            continue;
        }

        // Compare the modules with patterns based on the given module
        // instruction pair.
        if (PatternComps->first->compare() == 0
            && PatternComps->second->compare() == 0) {
            // Even if instructions match, the input synchronisation mapping
            // and arbitrary value and type correspondence need to be checked.
            if (!pairwiseMappingValid(InstPatternCompPair.first,
                                      PatternComps)) {
                continue;
            }

            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                            dbgs() << getDebugIndent()
                                   << "Found a match for instruction pattern "
                                   << InstPatternCompPair.first->Name << "\n");

            // Create a new instruction mapping since the match is valid.
            InstMappings.clear();
            processPatternMatch(InstPatternCompPair.first, PatternComps);
            return true;
        }
    }
    return false;
}

/// Tries to match one of the loaded value patterns. Returns true if
/// a valid match is found.
bool PatternComparator::matchValuePattern(const Instruction *InstL,
                                          const Instruction *InstR) {
    // Ensure that a load instruction has been given. Value differences
    // in other kinds of instructions are handled separately during
    // standard value comparison.
    auto LoadL = dyn_cast<LoadInst>(InstL);
    auto LoadR = dyn_cast<LoadInst>(InstR);
    if (!LoadL && !LoadR) {
        return false;
    }

    // Try to find a value-based pattern describing the difference in
    // the given load instructions.
    for (auto &&ValuePatternCompPair : ValuePatternComps) {
        bool LeftMatched =
                matchLoadInst(LoadL, ValuePatternCompPair.first, true);
        bool RightMatched =
                matchLoadInst(LoadR, ValuePatternCompPair.first, false);

        // Register matched instructions.
        if (LeftMatched) {
            AllInstMatches.insert(InstL);
        }
        if (RightMatched) {
            AllInstMatches.insert(InstR);
        }

        // If both load instructions from the compared modules match, create
        // a mapping between them as well.
        if (LeftMatched && RightMatched) {
            InstMappings.clear();
            InstMappings[InstL] = InstR;
        }

        if (LeftMatched || RightMatched) {
            return true;
        }
    }
    return false;
}

/// Tries to match a load instruction to the start of the given value
/// pattern.
bool PatternComparator::matchLoadInst(const LoadInst *Load,
                                      const ValuePattern *Pat,
                                      bool IsLeftSide) {
    // Ensure that the load instruction is valid.
    if (!Load) {
        return false;
    }

    // A match can be found only if there is a global variable pointer on the
    // selected pattern side.
    auto PatternValue = IsLeftSide ? Pat->ValueL : Pat->ValueR;
    if (!isa<GlobalVariable>(PatternValue)) {
        return false;
    }

    // Compare the loaded global variable by name.
    return namesMatch(Load->getOperand(0)->getName(),
                      PatternValue->getName(),
                      IsLeftSide);
}

/// Check whether the input, value, and type mapping generated by the given
/// pattern function comparator pair is valid even when both compared modules
/// are analysed at once
bool PatternComparator::pairwiseMappingValid(
        const InstPattern *Pat, const InstPatternComparatorPair *PatternComps) {
    // Find mapped input values. Values that could not get matched during
    // pattern comparison will be found using module synchronization maps.
    // Input validity will be checked again for values obtained in this
    // manner.
    //
    // This has to be done to ensure that all input values refer to the
    // same values in the matching module code. For example, here we need
    // to ensure that "%struct.x* %0" and "i32 %1" refer to the same values
    // in both modules. Otherwise, the pattern match is not valid.
    //
    // define i1 @diffkemp.old(%struct.x*, i32) {
    //     %3 = icmp ne i32 %1, 0
    //     ret i1 %3
    // }
    //
    // define i1 @diffkemp.new(%struct.x*, i32) {
    //     call void @fun(%struct.x* %0)
    //     %3 = icmp ne i32 %1, 0
    //     ret i1 %3
    // }
    for (auto &&ArgPair : Pat->ArgumentMapping) {
        auto InputPairL =
                PatternComps->first->PatInputMatchMap.find(ArgPair.first);
        auto InputPairR =
                PatternComps->second->PatInputMatchMap.find(ArgPair.second);
        auto MapLE = PatternComps->first->PatInputMatchMap.end();
        auto MapRE = PatternComps->second->PatInputMatchMap.end();

        if (InputPairL != MapLE && InputPairR != MapRE) {
            // The mapped values may be global variables.
            auto GlobalL = const_cast<GlobalValue *>(
                    dyn_cast<GlobalValue>(InputPairL->second));
            auto GlobalR = const_cast<GlobalValue *>(
                    dyn_cast<GlobalValue>(InputPairR->second));
            if (GlobalL && GlobalR) {
                if (DiffFunctionComp->cmpGlobalValues(GlobalL, GlobalR)) {
                    return false;
                }
                continue;
            }

            if (DiffFunctionComp->cmpValuesByMapping(InputPairL->second,
                                                     InputPairR->second)) {
                // Alloca instructions may have been ignored and skipped. In
                // that case, they would not be mapped yet (but need to be
                // mapped to avoid missmatches).
                if (isa<AllocaInst>(InputPairL->second)
                    && isa<AllocaInst>(InputPairR->second)) {
                    InstMappings[cast<Instruction>(InputPairL->second)] =
                            cast<Instruction>(InputPairR->second);
                    continue;
                }
                return false;
            }
        } else if (InputPairL != MapLE) {
            auto MappedR =
                    DiffFunctionComp->getMappedValue(InputPairL->second, true);
            if (PatternComps->second->cmpInputValues(MappedR, ArgPair.second)) {
                return false;
            }
        } else if (InputPairR != MapRE) {
            auto MappedL =
                    DiffFunctionComp->getMappedValue(InputPairR->second, false);
            if (PatternComps->first->cmpInputValues(MappedL, ArgPair.first)) {
                return false;
            }
        }
    }

    // Check that arbitrary values got mapped to the same value in both modules.
    // Arbitrary values are also used for function name regex validation.
    for (auto &&ArbitraryValuePairL :
         PatternComps->first->ArbitraryValueMatchMap) {
        // Find the matching value from the second module.
        auto ArbitraryConst = ArbitraryValuePairL.first;
        if (PatternComps->second->ArbitraryValueMatchMap.find(ArbitraryConst)
            == PatternComps->second->ArbitraryValueMatchMap.end()) {
            // For regex-based function name matches, require that the
            // corresponding global constant got matched on both sides (it
            // should be unique).
            if (isa<CallInst>(ArbitraryValuePairL.second)) {
                return false;
            }
            // For other arbitrary value matches are considered valid when
            // matched in a single module (no counterpart exists).
            continue;
        }
        if (isa<CallInst>(ArbitraryValuePairL.second)) {
            continue;
        }

        // Ensure the mapped values are the same.
        auto MappedValueL = ArbitraryValuePairL.second;
        auto MappedValueR =
                PatternComps->second->ArbitraryValueMatchMap[ArbitraryConst];
        if (DiffFunctionComp->cmpArbitraryValueMatch(MappedValueL,
                                                     MappedValueR)) {
            return false;
        }
    }

    // Check that arbitrary types got mapped to the same type in both modules.
    for (auto &&ArbitraryTypePairL :
         PatternComps->first->ArbitraryTypeMatchMap) {
        // Find the matching type from the second module. Types without a
        // counterpart are considered valid.
        auto ArbitraryType = ArbitraryTypePairL.first;
        if (PatternComps->second->ArbitraryTypeMatchMap.find(ArbitraryType)
            == PatternComps->second->ArbitraryTypeMatchMap.end()) {
            continue;
        }

        // Ensure the mapped types are the same.
        auto MappedTypeL = ArbitraryTypePairL.second;
        auto MappedTypeR =
                PatternComps->second->ArbitraryTypeMatchMap[ArbitraryType];
        if (MappedTypeL->isStructTy() && MappedTypeR->isStructTy()) {
            auto StrippedNameL = dropSuffixes(MappedTypeL->getStructName());
            auto StrippedNameR = dropSuffixes(MappedTypeR->getStructName());
            if (StrippedNameL != StrippedNameR) {
                return false;
            }
        } else if (DiffFunctionComp->cmpTypes(MappedTypeL, MappedTypeR)) {
            return false;
        }
    }

    return true;
}

/// Create the resulting instruction mapping and add all matched
/// instructions into the combined instruction set.
void PatternComparator::processPatternMatch(
        const InstPattern *Pat, const InstPatternComparatorPair *PatternComps) {
    for (auto &&InstPair : PatternComps->first->InstMatchMap) {
        // Add the matched instruction into the set of matched instructions.
        AllInstMatches.insert(InstPair.second);

        // If the matched instruction is mapped, prepare a mapping between
        // the respective module instructions as well.
        if (Pat->OutputMapping.find(InstPair.first)
            != Pat->OutputMapping.end()) {
            auto MappedInstR = Pat->OutputMapping[InstPair.first];
            auto MappedInstL = PatternComps->second->InstMatchMap[MappedInstR];
            InstMappings[MappedInstL] = InstPair.second;
        }
    }

    // Process the matched instructions from the second pattern side.
    for (auto &&InstPair : PatternComps->second->InstMatchMap) {
        AllInstMatches.insert(InstPair.second);
    }
}
