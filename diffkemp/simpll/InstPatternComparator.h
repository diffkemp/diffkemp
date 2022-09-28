//===----- InstPatternComparator.h - Code pattern instruction matcher -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the LLVM code pattern matcher. The
/// pattern matcher is a comparator extension of the LLVM FunctionComparator
/// tailored to comparison of general instruction-based patterns.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_INSTPATTERNCOMPARATOR_H
#define DIFFKEMP_SIMPLL_INSTPATTERNCOMPARATOR_H

#include "FunctionComparator.h"
#include "PatternSet.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Extension of LLVM FunctionComparator which compares a difference pattern
/// against its corresponding module function. Compared functions are expected
/// to lie in different modules. Only one side of an instruction pattern can
/// be compared at once. Therefore, it is expected that the instances of
/// InstPatternComparator will be used in pairs (one for each side of the
/// compared pattern).
class InstPatternComparator : protected FunctionComparator {
  public:
    /// The staring instruction of the compared module function.
    mutable const Instruction *StartInst;
    /// Pattern instructions matched to their respective module replacement
    /// instructions. Pattern instructions are used as keys.
    mutable InstructionMap InstMatchMap;
    /// Pattern input arguments matched to module input arguments. Pattern input
    /// is used for keys.
    mutable Pattern::ValueMap PatInputMatchMap;
    /// Module input arguments matched to pattern input arguments. A reverse of
    /// PatInputMatchMap necessary for computational purposes. Hence, module
    /// input is used for keys.
    mutable Pattern::ValueMap ModInputMatchMap;
    /// Arbitrary pattern values matched to module value counterparts. These can
    /// be constants marked as arbitrary, or values loaded from such constants,
    /// or marked as arbitrary by metadata. Pattern values are used as keys.
    mutable Pattern::ValueMap ArbitraryValueMatchMap;
    /// Arbitrary pattern types matched to module type counterparts. Pattern
    /// types are used as keys.
    mutable Pattern::TypeMap ArbitraryTypeMatchMap;

    InstPatternComparator(const Function *ModFun,
                          const Function *PatFun,
                          const InstPattern *ParentPattern)
            : FunctionComparator(ModFun, PatFun, nullptr),
              IsLeftSide(PatFun == ParentPattern->PatternL),
              ParentPattern(ParentPattern){};

    /// Compare the module function and the difference pattern from the starting
    /// module instruction.
    int compare() override;

    /// Compare the starting module instruction with the starting pattern
    /// instruction.
    int compareStartInst();

    /// Compare a module input value with a pattern input value.
    int cmpInputValues(const Value *ModVal, const Value *PatVal);

  protected:
#if LLVM_VERSION_MAJOR >= 13
    /// Always compare attributes as equal when using LLVM 13 (necessary due to
    /// a probable bug in LLVM 13).
    int cmpAttrs(const AttributeList ModAttrs,
                 const AttributeList PatAttrs) const override;
#endif

    /// Reset the comparison.
    void beginCompare(bool ClearMatchState);

    /// Compare a module instruction with a pattern instruction while ignoring
    /// alignment if not disabled.
    int cmpOperations(const Instruction *ModInst,
                      const Instruction *PatInst,
                      bool &needToCmpOperands) const override;

    /// Compare a module GEP operation with a pattern GEP operation, handling
    /// arbitrary indices.
    int cmpGEPs(const GEPOperator *ModGEP,
                const GEPOperator *PatGEP) const override;

    /// Compares a module type with a pattern type using name-only comparison
    /// for structured types, and handling arbitrary types.
    int cmpTypes(Type *ModTy, Type *PatTy) const override;

    /// Compare a module function instruction with a pattern instruction along
    /// with their operands.
    int cmpOperationsWithOperands(const Instruction *ModInst,
                                  const Instruction *PatInst) const;

    /// Compare a module function basic block with a pattern basic block.
    int cmpBasicBlocks(const BasicBlock *ModBB,
                       const BasicBlock *PatBB) const override;

    /// Compare global values by their names if not disabled, because their
    /// indexes are not expected to be the same.
    int cmpGlobalValues(GlobalValue *ModVal,
                        GlobalValue *PatVal) const override;

    /// Compare a module value with a pattern value using serial numbers,
    /// handling arbitrary values.
    int cmpValues(const Value *ModVal, const Value *PatVal) const override;

  private:
    /// Set of mapped and synchronised values.
    using ValueMappingSet = SmallPtrSet<const Value *, 16>;

    /// Set of mapped arbitrary types.
    using TypeMappingSet = SmallPtrSet<const Type *, 8>;

    /// Whether the comparator has been created for the left pattern side.
    const bool IsLeftSide;
    /// The pattern which should be used during comparison.
    const InstPattern *ParentPattern;
    /// Current position in the the compared module function.
    mutable const Instruction *ModPosition;
    /// Current position in the the compared pattern function.
    mutable const Instruction *PatPosition;
    /// Values placed into synchronisation maps during the comparison of the
    /// current instruction pair.
    mutable ValueMappingSet NewlyMappedModValues, NewlyMappedPatValues;
    /// Input instructions that have been mapped during the comparison of the
    /// current instruction pair.
    mutable ValueMappingSet NewlyMappedModInput, NewlyMappedPatInput;
    /// Arbitrary values that have been mapped to module counterparts during the
    /// comparison of the current instruction pair.
    mutable ValueMappingSet NewlyMappedArbitraryValues;
    /// Arbitrary types that have been mapped to module counterparts during the
    /// comparison of the current instruction pair.
    mutable TypeMappingSet NewlyMappedArbitraryTypes;
    /// Current instruction group depth.
    mutable int GroupDepth = 0;
    /// Whether the name-only comparison of structures and global variables is
    /// enabled.
    mutable bool NameComparisonEnabled = true;
    /// Whether alignment comparison is enabled.
    mutable bool AlignComparisonEnabled = false;

    /// Uses function comparison to try and match the given pattern to the
    /// corresponding module.
    int matchPattern() const;

    /// Erases newly mapped instructions from synchronization maps and input
    /// maps.
    void eraseNewlyMapped() const;

    /// Checks whether all currently mapped input instructions or arguments have
    /// an associated module counterpart.
    int checkInputMapping() const;

    /// Tries to map a module value (including possible predecessors) to a
    /// pattern input value.
    int mapInputValues(const Value *ModVal, const Value *PatVal) const;

    /// Checks whether the given instruction contains metadata marking the end
    /// of a pattern.
    bool hasPatternEnd(const Instruction *Inst) const;

    /// Updates the global instruction group depth in accordance to the metadata
    /// of the given instruction.
    void updateGroupDepth(const Instruction *Inst) const;

    /// Updates toggleable comparison states in accordance to the metadata of
    /// the given instruction.
    void updateCompareToggles(const Instruction *Inst) const;

    /// Position the basic block instruction iterator forward to the given
    /// instruction.
    void jumpToInst(BasicBlock::const_iterator &BBIterator,
                    const Instruction *Start) const;
};

#endif // DIFFKEMP_SIMPLL_INSTPATTERNCOMPARATOR_H
