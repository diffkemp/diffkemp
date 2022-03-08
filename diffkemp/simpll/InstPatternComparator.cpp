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
#include <llvm/Support/Regex.h>

/// Compare the module function and the difference pattern from the starting
/// module instruction. This includes checks for correct input mappings.
int InstPatternComparator::compare() {
    // Clear all previous results.
    beginCompare(true);

    // Run the main matching algorithm.
    if (int Res = matchPattern())
        return Res;

    // Reset the comparison state without clearing pattern matches.
    beginCompare(false);

    // Ensure that the created input mapping is correct. All input instructions
    // and arguments have to be mapped correctly.
    if (int Res = checkInputMapping())
        return Res;

    return 0;
}

/// Compare the starting module instruction with the starting pattern
/// instruction.
int InstPatternComparator::compareStartInst() {
    // Clear all previous results.
    beginCompare(true);

    auto StartInstPat = IsLeftSide ? ParentPattern->StartPositionL
                                   : ParentPattern->StartPositionR;

    // Process relevant pattern metadata.
    if (hasPatternEnd(StartInstPat))
        return 0;
    updateCompareToggles(StartInstPat);

    // Try to match the starting instructions.
    return cmpOperationsWithOperands(StartInst, StartInstPat);
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
    beginCompare(false);

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

/// Reset the comparison.
void InstPatternComparator::beginCompare(bool ClearMatchState) {
    sn_mapL.clear();
    sn_mapR.clear();

    if (ClearMatchState) {
        InstMatchMap.clear();
        PatInputMatchMap.clear();
        ModInputMatchMap.clear();
        ArbitraryValueMatchMap.clear();
        ArbitraryTypeMatchMap.clear();
    }
}

/// Compare a module instruction with a pattern instruction while ignoring
/// alignment of alloca, load, and store instructions if not disabled.
/// Note: Parts of this function have been adapted from FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information in
/// the appropriate llvm-lib subdirectory for more details.
int InstPatternComparator::cmpOperations(const Instruction *ModInst,
                                         const Instruction *PatInst,
                                         bool &needToCmpOperands) const {
    // Compare alloca, load, and store instructions without alignment if not
    // disabled.
    if (!AlignComparisonEnabled) {
        needToCmpOperands = true;
        // Compare information shared accross instruction types.
        if (int Res = cmpValues(ModInst, PatInst))
            return Res;

        if (int Res = cmpNumbers(ModInst->getOpcode(), PatInst->getOpcode()))
            return Res;

        if (int Res = cmpTypes(ModInst->getType(), PatInst->getType()))
            return Res;

        if (int Res = cmpNumbers(ModInst->getNumOperands(),
                                 PatInst->getNumOperands()))
            return Res;

        if (int Res = cmpNumbers(ModInst->getRawSubclassOptionalData(),
                                 PatInst->getRawSubclassOptionalData()))
            return Res;

        // Compare operand types.
        for (unsigned i = 0, e = ModInst->getNumOperands(); i != e; ++i) {
            if (int Res = cmpTypes(ModInst->getOperand(i)->getType(),
                                   PatInst->getOperand(i)->getType()))
                return Res;
        }

        // Compare the instructions based on their type.
        if (const AllocaInst *AI = dyn_cast<AllocaInst>(ModInst)) {
            return cmpTypes(AI->getAllocatedType(),
                            cast<AllocaInst>(PatInst)->getAllocatedType());
        }

        if (const LoadInst *LI = dyn_cast<LoadInst>(ModInst)) {
            if (int Res = cmpNumbers(LI->isVolatile(),
                                     cast<LoadInst>(PatInst)->isVolatile()))
                return Res;
            if (int Res = cmpOrderings(LI->getOrdering(),
                                       cast<LoadInst>(PatInst)->getOrdering()))
                return Res;
            if (int Res = cmpNumbers(LI->getSyncScopeID(),
                                     cast<LoadInst>(PatInst)->getSyncScopeID()))
                return Res;
            return cmpRangeMetadata(LI->getMetadata(LLVMContext::MD_range),
                                    cast<LoadInst>(PatInst)->getMetadata(
                                            LLVMContext::MD_range));
        }

        if (const StoreInst *SI = dyn_cast<StoreInst>(ModInst)) {
            if (int Res = cmpNumbers(SI->isVolatile(),
                                     cast<StoreInst>(PatInst)->isVolatile()))
                return Res;
            if (int Res = cmpOrderings(SI->getOrdering(),
                                       cast<StoreInst>(PatInst)->getOrdering()))
                return Res;
            return cmpNumbers(SI->getSyncScopeID(),
                              cast<StoreInst>(PatInst)->getSyncScopeID());
        }
    }

    return FunctionComparator::cmpOperations(
            ModInst, PatInst, needToCmpOperands);
}

/// Compare a module GEP operation with a pattern GEP operation, handling
/// arbitrary indices. The implementation is extended to support a name-based
/// comparison of structure types.
/// Note: Parts of this function have been adapted from FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information in
/// the appropriate llvm-lib subdirectory for more details.
int InstPatternComparator::cmpGEPs(const GEPOperator *ModGEP,
                                   const GEPOperator *PatGEP) const {
    // When using the GEP operations on pointers, vectors or arrays, perform the
    // default comparison. Also use the default comparison if the name-based
    // structure comparison is disabled.
    if (!ModGEP->getSourceElementType()->isStructTy()
        || !PatGEP->getSourceElementType()->isStructTy()
        || !NameComparisonEnabled) {
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

    if (int Res = cmpTypes(ModTy, PatTy)) {
        return Res;
    }

    if (int Res =
                cmpNumbers(ModGEP->getNumOperands(), PatGEP->getNumOperands()))
        return Res;

    // If the GEP index is arbitrary, match it to the linked global constant.
    bool ArbitraryIndex = false;
    auto PatGEPMetadata = ParentPattern->MetadataMap.find(PatGEP);
    if (PatGEPMetadata != ParentPattern->MetadataMap.end()
        && PatGEPMetadata->second.ArbitraryGEPConst
        && PatGEP->getNumOperands() > 2) {
        ArbitraryIndex = true;

        // Process arbitrary pattern values. These can be constants marked as
        // arbitrary, or values loaded from such constants,. Drop suffixes to
        // allow for multiple arbitrary value constants in one pattern.
        // If this is the first occurrence, only register and synchronise the
        // corresponding module value. Use the original constant for mapping.
        auto ModVal = ModGEP->getOperand(2);
        auto PatVal = PatGEP->getOperand(2);
        auto PatConst = PatGEPMetadata->second.ArbitraryGEPConst;
        if (ArbitraryValueMatchMap.insert({PatConst, ModVal}).second) {
            NewlyMappedArbitraryValues.insert(PatConst);
        } else {
            // For subsequent occurrences of the value, ensure that the matching
            // module value is the same.
            if (ModVal != ArbitraryValueMatchMap[PatConst]) {
                return 1;
            }
        }
    }

    for (unsigned i = 0, e = ModGEP->getNumOperands(); i != e; ++i) {
        // Only compare operands that are not arbitrary.
        if (ArbitraryIndex && i == 2)
            continue;
        if (int Res = cmpValues(ModGEP->getOperand(i), PatGEP->getOperand(i)))
            return Res;
    }

    return 0;
}

/// Compares a module type with a pattern type using name-only comparison
/// for structured types, and handling arbitrary types.
int InstPatternComparator::cmpTypes(Type *ModTy, Type *PatTy) const {
    // Check for arbitrary types on the pattern side. Note that pointers need to
    // be analysed as well since pointer type naming is normally ignored.
    if (PatTy->isStructTy() || (ModTy->isPointerTy() && PatTy->isPointerTy())) {
        // Synchronously strip pointers.
        Type *StrippedModTy = ModTy;
        Type *StrippedPatTy = PatTy;
        while (StrippedPatTy->isPointerTy()) {
            if (!StrippedModTy->isPointerTy())
                return FunctionComparator::cmpTypes(ModTy, PatTy);
            StrippedPatTy = StrippedPatTy->getPointerElementType();
            StrippedModTy = StrippedModTy->getPointerElementType();
        }

        // Match arbitrary types. Drop suffixes to allow for multiple arbitrary
        // types in one pattern.
        if (StrippedPatTy->isStructTy()
            && dropSuffixes(StrippedPatTy->getStructName().str())
                       == PatternSet::ArbitraryTypeStructName) {
            // If this is the first occurrence, only register the corresponding
            // module type.
            if (ArbitraryTypeMatchMap.insert({StrippedPatTy, StrippedModTy})
                        .second) {
                NewlyMappedArbitraryTypes.insert(StrippedPatTy);
                return 0;
            }

            // For subsequent occurrences of the type, ensure that the matching
            // module type is the same.
            return FunctionComparator::cmpTypes(
                    StrippedModTy, ArbitraryTypeMatchMap[StrippedPatTy]);
        }
    }

    // Try the name-only comparison if not disabled.
    if (ModTy->isStructTy() && PatTy->isStructTy() && NameComparisonEnabled
        && namesMatch(
                ModTy->getStructName(), PatTy->getStructName(), IsLeftSide)) {
        return 0;
    }

    return FunctionComparator::cmpTypes(ModTy, PatTy);
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
    NewlyMappedArbitraryValues.clear();
    NewlyMappedArbitraryTypes.clear();

    // Compare the instruction and its operands.
    if (int Res = cmpOperations(&*ModInst, &*PatInst, NeedToCmpOperands))
        return Res;
    if (NeedToCmpOperands) {
        assert(ModInst->getNumOperands() == PatInst->getNumOperands());

        for (unsigned i = 0, e = ModInst->getNumOperands(); i != e; ++i) {
            Value *ModOp = ModInst->getOperand(i);
            Value *PatOp = PatInst->getOperand(i);

            if (int Res = cmpValues(ModOp, PatOp)) {
                // Try to match function call names according to regular
                // expressions from metadata.
                auto PatMetadata = ParentPattern->MetadataMap.find(PatInst);
                auto ModCall = dyn_cast<CallInst>(ModInst);
                if (ModCall && isa<CallInst>(PatInst)
                    && ModOp == ModCall->getCalledOperand()
                    && PatMetadata != ParentPattern->MetadataMap.end()) {
                    auto CalledModFunction = ModCall->getCalledFunction();
                    if (!CalledModFunction)
                        return Res;

                    for (auto &&RegexPair :
                         PatMetadata->second.FunctionNameRegexes) {
                        auto CompiledRegex = Regex(RegexPair.first);
                        if (CompiledRegex.match(CalledModFunction->getName())) {
                            // Map the matching call to the corresponding global
                            // constant.
                            if (ArbitraryValueMatchMap
                                        .insert({RegexPair.second, ModInst})
                                        .second) {
                                NewlyMappedArbitraryValues.insert(
                                        RegexPair.second);
                                Res = 0;
                                break;
                            }
                        }
                    }
                }
                if (Res)
                    return Res;
            }
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

        // Update toggleable comparison states.
        updateCompareToggles(&*PatInst);

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

/// Compare global values by their names if not disabled, because their indexes
/// are not expected to be the same.
int InstPatternComparator::cmpGlobalValues(GlobalValue *ModVal,
                                           GlobalValue *PatVal) const {
    if (!NameComparisonEnabled)
        return FunctionComparator::cmpGlobalValues(ModVal, PatVal);

    // When enabled, compare global values by name.
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

/// Compare a module value with a pattern value using serial numbers, handling
/// arbitrary values.
int InstPatternComparator::cmpValues(const Value *ModVal,
                                     const Value *PatVal) const {
    // Process arbitrary pattern values. These can be constants marked as
    // arbitrary, or values loaded from such constants. Drop suffixes to
    // allow for multiple arbitrary value constants in one pattern.
    const Constant *PatConst = dyn_cast<Constant>(PatVal);
    auto StrippedName = dropSuffixes(PatVal->getName());
    bool IsArbitrary = ParentPattern->ArbitraryValues.find(PatVal)
                       != ParentPattern->ArbitraryValues.end();
    if ((PatConst && StrippedName == PatternSet::ArbitraryValueConstName)
        || IsArbitrary) {
        // Get the corresponding arbitrary constant for values loaded from them.
        if (!PatConst) {
            PatConst = ParentPattern->ArbitraryValues[PatVal];
        }

        // If this is the first occurrence, only register and synchronise the
        // corresponding module value. Use the original constant for mapping.
        if (ArbitraryValueMatchMap.insert({PatConst, ModVal}).second) {
            NewlyMappedArbitraryValues.insert(PatConst);
            return 0;
        }
        // For subsequent occurrences of the value, ensure that the matching
        // module value is the same.
        return ModVal != ArbitraryValueMatchMap[PatConst];
    }

    // Perform the default value comparison.
    int Result = FunctionComparator::cmpValues(ModVal, PatVal);

    // Module constants can be mapped to input arguments (but not input
    // instructions).
    const Constant *ModConst = dyn_cast<Constant>(ModVal);
    const Instruction *PatInst = dyn_cast<Instruction>(PatVal);
    auto PatternInput =
            IsLeftSide ? ParentPattern->InputL : ParentPattern->InputR;
    if (ModConst && !PatConst && !PatInst
        && PatternInput.find(PatVal) != PatternInput.end()) {
        auto ModSN = sn_mapL.insert(std::make_pair(ModVal, sn_mapL.size())),
             PatSN = sn_mapR.insert(std::make_pair(PatVal, sn_mapR.size()));
        Result = cmpNumbers(ModSN.first->second, PatSN.first->second);
    }

    if (Result)
        return Result;

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
        NameComparisonEnabled = true;
        AlignComparisonEnabled = false;
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
    for (auto &&MappedArbitraryValue : NewlyMappedArbitraryValues) {
        NewlyMappedArbitraryValues.erase(MappedArbitraryValue);
    }
    for (auto &&MappedArbitraryType : NewlyMappedArbitraryTypes) {
        NewlyMappedArbitraryTypes.erase(MappedArbitraryType);
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

/// Updates toggleable comparison states in accordance to the metadata of the
/// given instruction.
void InstPatternComparator::updateCompareToggles(
        const Instruction *Inst) const {
    auto InstMetadata = ParentPattern->MetadataMap.find(Inst);
    if (InstMetadata != ParentPattern->MetadataMap.end()) {
        if (InstMetadata->second.EnableNameComparison)
            NameComparisonEnabled = true;
        if (InstMetadata->second.DisableNameComparison)
            NameComparisonEnabled = false;
        if (InstMetadata->second.EnableAlignComparison)
            AlignComparisonEnabled = true;
        if (InstMetadata->second.DisableAlignComparison)
            AlignComparisonEnabled = false;
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
