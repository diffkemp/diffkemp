//===---- InstPatternComparator.cpp - Code pattern instruction matcher ----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the LLVM code pattern matcher. The
/// pattern matcher is a comparator extension of the LLVM FunctionComparator
/// tailored to comparison of general instruction-based patterns.
///
//===----------------------------------------------------------------------===//

#include "InstPatternComparator.h"
#include "Config.h"

/// Compare the module function and the difference pattern from the starting
/// module instruction. This includes checks for correct input mappings.
int InstPatternComparator::compare() {
    // Clear all previous results.
    beginCompare();
    InstMatchMap.clear();
    PatInputMatchMap.clear();
    ModInputMatchMap.clear();

    // Run the main matching algorithm.
    if (int Res = matchPattern())
        return Res;

    // Reset the comparison state without clearing pattern matches.
    beginCompare();

    // Ensure that the created input mapping is correct. All input instructions
    // and arguments have to be mapped correctly.
    if (int Res = checkInputMapping())
        return Res;

    return 0;
}

/// Compare a module input value with a pattern input value. Used for comparing
/// input values that could not be mapped during the first one-side comparison.
int InstPatternComparator::cmpInputValues(const Value *ModVal,
                                          const Value *PatVal) {
    // Check pointer validity.
    if (!ModVal)
        return -1;
    if (!PatVal)
        return 1;

    // The pattern value may have been already mapped. If so, it must be mapped
    // to the given module value.
    if (PatInputMatchMap.find(PatVal) != PatInputMatchMap.end()) {
        if (PatInputMatchMap[PatVal] != ModVal)
            return 1;
        return 0;
    }

    // Reset the comparison state without clearing pattern matches.
    beginCompare();

    SmallVector<const Value *, 8> ModInput, PatInput;
    SmallPtrSet<const Value *, 32> ModVisited;

    // Initialize the input comparison with the given values.
    ModInput.push_back(ModVal);
    PatInput.push_back(PatVal);

    // Try to find a matching module counterpart for all instructions that use
    // the compared pattern value (either directly or indirectly).
    while (!ModInput.empty()) {
        auto CurrModVal = ModInput.pop_back_val();
        auto CurrPatVal = PatInput.pop_back_val();

        // Map the values to each other.
        PatInputMatchMap[PatVal] = ModVal;
        ModInputMatchMap[ModVal] = PatVal;
        ModVisited.insert(ModVal);

        auto ModUser = CurrModVal->user_begin(),
             PatUser = CurrPatVal->user_begin();
        auto ModUserB = ModUser;
        auto ModUserE = CurrModVal->user_end(),
             PatUserE = CurrPatVal->user_end();

        while (ModUser != ModUserE && PatUser != PatUserE) {
            // Skip pattern users that do not represent input values.
            auto PatUserMetadata = ParentPattern->MetadataMap.find(*PatUser);
            if (PatUserMetadata != ParentPattern->MetadataMap.end()
                && PatUserMetadata->second.NotAnInput) {
                ++PatUser;
                continue;
            }

            auto ModInst = dyn_cast<Instruction>(*ModUser);
            auto PatInst = dyn_cast<Instruction>(*PatUser);

            // Users are expected to be instructions.
            if (!ModInst) {
                ++ModUser;
                continue;
            }
            if (!PatInst)
                return 1;

            if (ModInputMatchMap.find(ModInst) != ModInputMatchMap.end()) {
                // Skip already mapped module instructions.
                if (ModInputMatchMap[ModInst] != PatInst) {
                    ++ModUser;
                    continue;
                }
            } else if (PatInputMatchMap.find(PatInst)
                       != PatInputMatchMap.end()) {
                // Skip pattern instructions that have already been mapped to
                // one of the analysed module instructions.
                if (std::find(ModUserB, ModUserE, PatInputMatchMap[PatInst])
                    != ModUserE) {
                    ++PatUser;
                    continue;
                }
                // When mapped to an unrelated instruction, fail the comparison.
                break;
            } else if (cmpOperationsWithOperands(ModInst, PatInst)) {
                // Compare the instructions. If they match, continue the
                // comparison on both sides. Otherwise, try to find a different,
                // more suitable module instruction.
                eraseNewlyMapped();
                ++ModUser;
                continue;
            }

            // A match between both users has been found. Increment both
            // iterators.
            ++ModUser;
            ++PatUser;

            if (!ModVisited.insert(ModInst).second)
                continue;

            // Schedule newly mapped instructions for comparison.
            ModInput.push_back(ModInst);
            PatInput.push_back(PatInst);
        }

        // If any users remain on the pattern side, ensure that they are mapped
        // to skipped users from the module.
        while (PatUser != PatUserE) {
            if (std::find(ModUserB, ModUserE, PatInputMatchMap[*PatUser])
                == ModUserE)
                return -1;

            ++PatUser;
        }
    }

    return 0;
}

#if LLVM_VERSION_MAJOR == 13
/// Always compare attributes as equal when using LLVM 13 (necessary due to a
/// probable bug in LLVM 13).
int InstPatternComparator::cmpAttrs(const AttributeList /* ModAttrs */,
                                    const AttributeList /* PatAttrs */) const {
    return 0;
}
#endif

/// Compare a module GEP operation with a pattern GEP operation. The
/// implementation is extended to support a name-based comparison of structure
/// types.
/// Note: Parts of this function have been adapted from FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information in
/// the appropriate llvm-lib subdirectory for more details.
int InstPatternComparator::cmpGEPs(const GEPOperator *ModGEP,
                                   const GEPOperator *PatGEP) const {
    // When using the GEP operations on pointers, vectors or arrays, perform the
    // default comparison. Also use the default comparison if the name-based
    // structure comparison is disabled.
    auto PatGEPMetadata = ParentPattern->MetadataMap.find(PatGEP);
    if (!ModGEP->getSourceElementType()->isStructTy()
        || !PatGEP->getSourceElementType()->isStructTy()
        || (PatGEPMetadata != ParentPattern->MetadataMap.end()
            && PatGEPMetadata->second.DisableNameComparison)) {
        return FunctionComparator::cmpGEPs(ModGEP, PatGEP);
    }

    // Compare structures without calculating offsets since both structures
    // should be the same.
    unsigned int ModAS = ModGEP->getPointerAddressSpace();
    unsigned int PatAS = PatGEP->getPointerAddressSpace();

    Type *ModTy = ModGEP->getSourceElementType();
    Type *PatTy = PatGEP->getSourceElementType();

    if (int Res = cmpNumbers(ModAS, PatAS))
        return Res;

    // Try to perform the base comparison. Validate the result by comparing the
    // structure names.
    if (int Res = cmpTypes(ModTy, PatTy)) {
        if (!namesMatch(
                    ModTy->getStructName(), PatTy->getStructName(), IsLeftSide))
            return Res;
    }

    if (int Res =
                cmpNumbers(ModGEP->getNumOperands(), PatGEP->getNumOperands()))
        return Res;

    for (unsigned i = 0, e = ModGEP->getNumOperands(); i != e; ++i) {
        if (int Res = cmpValues(ModGEP->getOperand(i), PatGEP->getOperand(i)))
            return Res;
    }

    return 0;
}

/// Compare a module function instruction with a pattern instruction along
/// with their operands.
/// Note: Parts of this function have been adapted from FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information
/// in the appropriate llvm-lib subdirectory for more details.
int InstPatternComparator::cmpOperationsWithOperands(
        const Instruction *ModInst, const Instruction *PatInst) const {
    bool NeedToCmpOperands = true;

    // Clear newly mapped holders.
    NewlyMappedModValues.clear();
    NewlyMappedPatValues.clear();
    NewlyMappedModInput.clear();
    NewlyMappedPatInput.clear();

    // Compare the instruction and its operands.
    if (int Res = cmpOperations(&*ModInst, &*PatInst, NeedToCmpOperands))
        return Res;
    if (NeedToCmpOperands) {
        assert(ModInst->getNumOperands() == PatInst->getNumOperands());

        for (unsigned i = 0, e = ModInst->getNumOperands(); i != e; ++i) {
            Value *ModOp = ModInst->getOperand(i);
            Value *PatOp = PatInst->getOperand(i);

            if (int Res = cmpValues(ModOp, PatOp))
                return Res;
            assert(cmpTypes(ModOp->getType(), PatOp->getType()) == 0);
        }
    }
    // Map the instructions to each other.
    InstMatchMap[PatInst] = ModInst;
    return 0;
}

/// Compare a module function basic block with a pattern basic block.
int InstPatternComparator::cmpBasicBlocks(const BasicBlock *ModBB,
                                          const BasicBlock *PatBB) const {
    auto ModInst = ModBB->begin(), ModInstE = ModBB->end();
    auto PatInst = PatBB->begin(), PatInstE = PatBB->end();
    auto ModTerm = ModBB->getTerminator(), PatTerm = PatBB->getTerminator();

    // Jump to the currently compared instruction pair.
    jumpToInst(ModInst, ModPosition);
    jumpToInst(PatInst, PatPosition);

    while (ModInst != ModInstE && PatInst != PatInstE) {
        ModPosition = &*ModInst;
        PatPosition = &*PatInst;

        // Check whether the compared pattern ends at this instruction.
        if (hasPatternEnd(&*PatInst))
            return 0;

        // If at the end of only one basic block, leave the rest to
        // unconditionally connected basic blocks (if there are any).
        if (&*ModInst == ModTerm && &*PatInst != PatTerm
            && ModTerm->getNumSuccessors() == 1) {
            return 1;
        }
        if (&*ModInst != ModTerm && &*PatInst == PatTerm) {
            if (PatTerm->getNumSuccessors() == 1)
                return 0;

            // If no unconditional successor exists, skip to the terminator
            // instruction.
            ModInst = ModInstE;
            --ModInst;
            continue;
        }

        // Compare current instructions with operands.
        if (int Res = cmpOperationsWithOperands(&*ModInst, &*PatInst)) {
            // Remove newly added value and input mappings.
            eraseNewlyMapped();

            // When in an instruction group, do not allow module instruction
            // skipping.
            if (GroupDepth > 0)
                return Res;

            // Skip the module instruction.
            ++ModInst;
            continue;
        }

        // Update the depth of pattern instruction groups.
        updateGroupDepth(&*PatInst);

        ++ModInst;
        ++PatInst;
    }

    if (ModInst != ModInstE && PatInst == PatInstE)
        return 1;
    if (ModInst == ModInstE && PatInst != PatInstE)
        return -1;

    return 0;
}

/// Compare global values by their names, because their indexes are not
/// expected to be the same.
int InstPatternComparator::cmpGlobalValues(GlobalValue *ModVal,
                                           GlobalValue *PatVal) const {
    if (ModVal->hasName() && PatVal->hasName()) {
        // Both values are named, compare them by names
        auto ModName = ModVal->getName();
        auto PatName = PatVal->getName();

        if (namesMatch(ModName, PatName, IsLeftSide))
            return 0;

        return 1;
    }

    return ModVal != PatVal;
}

/// Compare a module value with a pattern value using serial numbers.
int InstPatternComparator::cmpValues(const Value *ModVal,
                                     const Value *PatVal) const {
    // Perform the default value comparison.
    if (int Res = FunctionComparator::cmpValues(ModVal, PatVal))
        return Res;

    // Register newly inserted values.
    if (sn_mapL[ModVal] == int(sn_mapL.size() - 1))
        NewlyMappedModValues.insert(ModVal);
    if (sn_mapR[PatVal] == int(sn_mapR.size() - 1))
        NewlyMappedPatValues.insert(PatVal);

    // Since the values are equal, try to match them as inputs.
    if (int Res = mapInputValues(ModVal, PatVal))
        return Res;

    return 0;
}

/// Uses function comparison to try and match the given pattern to the
/// corresponding module. Uses the implementation of the compare method from
/// LLVM FunctionComparator, extended to support comparisons starting from
/// specific instructions. Because of that, code referring to the comparison
/// of whole functions has also been removed. Note: Parts of this function
/// have been adapted from the compare method of FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information
/// in the appropriate llvm-lib subdirectory for more details.
int InstPatternComparator::matchPattern() const {
    // We do a CFG-ordered walk since the actual ordering of the blocks in
    // the linked list is immaterial. Our walk starts at the containing
    // blocks of the starting instructions, then takes each block from each
    // terminator in order. Instructions from the first pair of blocks that
    // are before the starting instructions will get ignored. As an
    // artifact, this also means that unreachable blocks are ignored. Basic
    // blocks that are connected by unconditional branches get treated as a
    // single basic block.
    SmallVector<const BasicBlock *, 8> ModFnBBs, PatFnBBs;
    SmallPtrSet<const BasicBlock *, 32> ModVisited, PatVisited;

    // Set starting basic blocks and positions.
    ModFnBBs.push_back(StartInst->getParent());
    ModPosition = StartInst;
    if (IsLeftSide) {
        PatFnBBs.push_back(ParentPattern->StartPositionL->getParent());
        PatPosition = ParentPattern->StartPositionL;
    } else {
        PatFnBBs.push_back(ParentPattern->StartPositionR->getParent());
        PatPosition = ParentPattern->StartPositionR;
    }

    // Run the pattern comparison.
    ModVisited.insert(ModFnBBs[0]);
    PatVisited.insert(PatFnBBs[0]);
    while (!ModFnBBs.empty()) {
        const BasicBlock *ModBB = ModFnBBs.pop_back_val();
        const BasicBlock *PatBB = PatFnBBs.pop_back_val();

        // Compare the first basic blocks in an unconditionally connected
        // group as values.
        if (int Res = cmpValues(ModBB, PatBB))
            return Res;

        // Compare unconditionally connected basic blocks at the same time.
        GroupDepth = 0;
        bool ResultFound = false;
        while (!ResultFound) {
            // Ensure position compatibility.
            if (ModPosition->getParent() != ModBB)
                ModPosition = &*ModBB->begin();
            if (PatPosition->getParent() != PatBB)
                PatPosition = &*PatBB->begin();

            int BBCmpRes = cmpBasicBlocks(ModBB, PatBB);
            auto ModSucc = ModBB->getSingleSuccessor();
            auto PatSucc = PatBB->getSingleSuccessor();

            // Basic block comparison should return 0 when the pattern block
            // gets fully matched, and a non-zero value when the pattern block
            // matching ends early due to a premature ending of the compared
            // module block. In both cases, the comparison may continue with
            // the following basic block if there is only a single successor.
            if (BBCmpRes) {
                if (!ModSucc || !ModVisited.insert(ModSucc).second)
                    return BBCmpRes;

                // If an unvisited single successor exists, compare against
                // it instead.
                ModBB = ModSucc;
                ModPosition = &*ModBB->begin();
            } else {
                // If an unvisited single successor exists, descend into it.
                // Otherwise finalize the comparison of the current block.
                auto *PatTerm = PatBB->getTerminator();
                if (PatSucc && PatVisited.insert(PatSucc).second
                    && !hasPatternEnd(PatTerm)) {
                    PatBB = PatSucc;
                    PatPosition = &*PatBB->begin();
                } else {
                    ResultFound = true;
                }
            }
        }

        // Jump to the last unvisited module basic block in the current
        // unconditionally connected group.
        while (ModBB->getSingleSuccessor()) {
            ModBB = ModBB->getSingleSuccessor();
            if (!ModVisited.insert(ModBB).second)
                break;
        }

        auto *ModTerm = ModBB->getTerminator();
        auto *PatTerm = PatBB->getTerminator();

        // Do not descend to successors if the pattern terminates
        // in this basic block.
        if (hasPatternEnd(PatTerm) || PatTerm->getNumSuccessors() == 0)
            continue;

        // Queue all successor basic blocks.
        unsigned SuccLimit = std::min(ModTerm->getNumSuccessors(),
                                      PatTerm->getNumSuccessors());
        for (unsigned i = 0; i != SuccLimit; ++i) {
            if (!ModVisited.insert(ModTerm->getSuccessor(i)).second
                || !PatVisited.insert(PatTerm->getSuccessor(i)).second)
                continue;

            ModFnBBs.push_back(ModTerm->getSuccessor(i));
            PatFnBBs.push_back(PatTerm->getSuccessor(i));
        }
    }

    return 0;
}

/// Erases newly mapped instructions from synchronization maps and input
/// maps.
void InstPatternComparator::eraseNewlyMapped() const {
    for (auto &&MappedModValue : NewlyMappedModValues) {
        sn_mapL.erase(MappedModValue);
    }
    for (auto &&MappedPatValue : NewlyMappedPatValues) {
        sn_mapR.erase(MappedPatValue);
    }
    for (auto &&MappedModInput : NewlyMappedPatInput) {
        PatInputMatchMap.erase(MappedModInput);
    }
    for (auto &&MappedPatInput : NewlyMappedModInput) {
        ModInputMatchMap.erase(MappedPatInput);
    }
}

/// Checks whether all currently mapped input instructions or arguments have
/// an associated module counterpart.
int InstPatternComparator::checkInputMapping() const {
    // Compare mapped input arguments. Right side is the pattern side.
    for (auto &&PatArg : FnR->args()) {
        auto MappedValue = PatInputMatchMap.find(&PatArg);
        if (MappedValue != PatInputMatchMap.end()) {
            if (int Res = cmpValues(MappedValue->second, &PatArg))
                return Res;
        }
    }

    // Compare mapped input instructions. Corresponding instructions or
    // arguments should be present on the module side. Comparison starts
    // from the entry block since input instructions should be placed before
    // the instruction marked as pattern start.
    SmallVector<const BasicBlock *, 8> FnRBBs;
    SmallPtrSet<const BasicBlock *, 32> VisitedBBs;

    FnRBBs.push_back(&*FnR->begin());
    VisitedBBs.insert(FnRBBs[0]);
    while (!FnRBBs.empty()) {
        const BasicBlock *PatBB = FnRBBs.pop_back_val();

        bool PatternStartFound = false;
        for (auto &&PatInst : *PatBB) {
            auto PatInstMetadata = ParentPattern->MetadataMap.find(&PatInst);
            if (PatInstMetadata != ParentPattern->MetadataMap.end()) {
                // End after all input instructions have been processed.
                if (PatInstMetadata->second.PatternStart) {
                    PatternStartFound = true;
                    break;
                }

                // Only analyse input instructions.
                if (PatInstMetadata->second.NotAnInput)
                    continue;
            }

            auto MappedValues = PatInputMatchMap.find(&PatInst);
            if (MappedValues != PatInputMatchMap.end()) {
                // Use instruction comparison when mapped to an instruction.
                // Otherwise, only compare values.
                if (auto ModInst =
                            dyn_cast<Instruction>(MappedValues->second)) {
                    if (int Res = cmpOperationsWithOperands(ModInst, &PatInst))
                        return Res;
                } else if (int Res =
                                   cmpValues(MappedValues->second, &PatInst)) {
                    return Res;
                }
            }
        }

        // If this branch has not reached the starting pattern instruction yet,
        // analyse the following blocks.
        if (!PatternStartFound) {
            auto *PatBBTerm = PatBB->getTerminator();
            for (unsigned i = 0, e = PatBBTerm->getNumSuccessors(); i != e;
                 ++i) {
                if (!VisitedBBs.insert(PatBBTerm->getSuccessor(i)).second)
                    continue;

                FnRBBs.push_back(PatBBTerm->getSuccessor(i));
            }
        }
    }

    return 0;
}

/// Tries to map a module value (including possible predecessors) to a
/// pattern input value. If no input value is present, the mapping is
/// always successful.
int InstPatternComparator::mapInputValues(const Value *ModVal,
                                          const Value *PatVal) const {
    // The pattern input may have been already mapped. If so, it must be
    // mapped to the given module value.
    if (PatInputMatchMap.find(PatVal) != PatInputMatchMap.end()) {
        if (PatInputMatchMap[PatVal] != ModVal)
            return 1;
        return 0;
    }

    auto Input = IsLeftSide ? &ParentPattern->InputL : &ParentPattern->InputR;

    SmallVector<const Value *, 16> ModInput, PatInput;
    SmallPtrSet<const Value *, 32> ModVisited;

    // Initialize the input search with the given values.
    ModInput.push_back(ModVal);
    PatInput.push_back(PatVal);
    ModVisited.insert(ModVal);

    // If input values are given, map them to their module counterparts
    // (including predecessors).
    while (!ModInput.empty()) {
        auto CurrModVal = ModInput.pop_back_val();
        auto CurrPatVal = PatInput.pop_back_val();

        if (Input->find(CurrPatVal) == Input->end())
            continue;

        // Map the values together.
        PatInputMatchMap[CurrPatVal] = CurrModVal;
        ModInputMatchMap[CurrModVal] = CurrPatVal;
        NewlyMappedModInput.insert(CurrModVal);
        NewlyMappedPatInput.insert(CurrPatVal);

        // Descend only if both values are instructions.
        auto ModInst = dyn_cast<Instruction>(CurrModVal);
        auto PatInst = dyn_cast<Instruction>(CurrPatVal);
        if (ModInst && PatInst) {
            // Mapped input instructions should have the same number of
            // operands.
            if (int Res = cmpNumbers(ModInst->getNumOperands(),
                                     PatInst->getNumOperands()))
                return Res;

            // Descend into unvisited operands.
            for (int i = 0, e = ModInst->getNumOperands(); i != e; ++i) {
                Value *ModOp = ModInst->getOperand(i);
                Value *PatOp = PatInst->getOperand(i);

                if (PatInputMatchMap.find(PatOp) != PatInputMatchMap.end()
                    || !ModVisited.insert(ModOp).second)
                    continue;

                ModInput.push_back(ModOp);
                PatInput.push_back(PatOp);
            }
        }
    }

    return 0;
}

/// Checks whether the given instruction contains metadata marking the end
/// of a pattern.
bool InstPatternComparator::hasPatternEnd(const Instruction *Inst) const {
    auto InstMetadata = ParentPattern->MetadataMap.find(Inst);
    return (InstMetadata != ParentPattern->MetadataMap.end()
            && InstMetadata->second.PatternEnd);
}

/// Updates the global instruction group depth in accordance to the metadata
/// of the given instruction.
void InstPatternComparator::updateGroupDepth(const Instruction *Inst) const {
    auto InstMetadata = ParentPattern->MetadataMap.find(Inst);
    if (InstMetadata != ParentPattern->MetadataMap.end()) {
        if (InstMetadata->second.GroupStart)
            ++GroupDepth;
        if (InstMetadata->second.GroupEnd)
            --GroupDepth;
    }
}

/// Position the basic block instruction iterator forward to the given
/// instruction.
void InstPatternComparator::jumpToInst(BasicBlock::const_iterator &BBIterator,
                                       const Instruction *Inst) const {
    while (&*BBIterator != Inst) {
        ++BBIterator;
    }
}
