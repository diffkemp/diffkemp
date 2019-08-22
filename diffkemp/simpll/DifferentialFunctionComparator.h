//===- DifferentialFunctionComparator.h - Comparing functions for equality ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the DifferentialFunctionComparator
/// class that extends the FunctionComparator from the LLVM infrastructure for
/// the specific purposes of SimpLL.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_DIFFERENTIALFUNCTIONCOMPARATOR_H
#define DIFFKEMP_SIMPLL_DIFFERENTIALFUNCTIONCOMPARATOR_H

#include "DebugInfo.h"
#include "ModuleComparator.h"
#include "Utils.h"
#include "FunctionComparator.h"

using namespace llvm;

/// Extension of FunctionComparator from LLVM designed to compare functions in
/// different modules (original FunctionComparator assumes that both functions
/// are in a single module).

class DifferentialFunctionComparator : public FunctionComparator {
  public:
    DifferentialFunctionComparator(const Function *F1,
                                   const Function *F2,
                                   bool controlFlowOnly,
                                   bool showAsmDiff,
                                   const DebugInfo *DI,
                                   ModuleComparator *MC)
            : FunctionComparator(F1, F2, nullptr), DI(DI),
              controlFlowOnly(controlFlowOnly), showAsmDiff(showAsmDiff),
              LayoutL(F1->getParent()->getDataLayout()),
              LayoutR(F2->getParent()->getDataLayout()),
              ModComparator(MC) {}

  protected:
    /// Specific comparison of GEP instructions/operators.
    /// Handles situation when there is an offset between matching GEP indices
    /// in the compared modules (when a struct type has different fields).
    int cmpGEPs(const GEPOperator *GEPL,
                const GEPOperator *GEPR) const override;
    /// Specific comparison of attribute lists.
    /// Attributes that do not affect the semantics of functions are removed.
    int cmpAttrs(const AttributeList L, const AttributeList R) const override;
    /// Compare CallInsts using cmpAllocs.
    int cmpOperations(const Instruction *L, const Instruction *R,
                      bool &needToCmpOperands) const override;
    /// Handle comparing of memory allocation function in cases where the size
    /// of the composite type is different.
    int cmpAllocs(const CallInst *CL, const CallInst *CR) const;
    /// Compare two function calls where one has an extra argument.
    /// Such calls are compared as equal if they only differ in the last
    /// argument which is 0 or NULL.
    int cmpCallsWithExtraArg(const CallInst *CL, const CallInst *CR) const;
    /// Compares array types with equivalent element types and all integer types
    /// as equal when comparing the control flow only.
    int cmpTypes(Type *L, Type *R) const override;
    /// Do not compare bitwidth when comparing the control flow only.
    int cmpAPInts(const APInt &L, const APInt &R) const override;
    /// Detect cast instructions and ignore them when comparing the control flow
    /// only. (The rest is the same as in LLVM.)
    int cmpBasicBlocks(const BasicBlock *BBL, const BasicBlock *BBR)
        const override;
    /// Implement comparison of global values that does not use a
    /// GlobalNumberState object, since that approach does not fit the use case
    /// of comparing functions in two different modules.
    int cmpGlobalValues(GlobalValue *L, GlobalValue *R) const override;
    /// Specific comparing of values. Handles values generated from macros
    /// whose value changed and values where at least one of them is a cast.
    int cmpValues(const Value *L, const Value *R) const override;
    /// Specific comparing of constants. If one of them (or both) is a cast
    /// constant expression, compare its operand.
    int cmpConstants(const Constant *L, const Constant *R) const override;
    /// Specific comarison of memcpy instructions
    int cmpMemset(const CallInst *CL, const CallInst *CR) const;
    /// Comparing of a structure size with a constant
    int cmpStructTypeSizeWithConstant(StructType *Type,
                                      const Value *Const) const;

  private:
    const DebugInfo *DI;
    bool controlFlowOnly, showAsmDiff;

    const DataLayout &LayoutL, &LayoutR;

    ModuleComparator *ModComparator;

    /// Looks for inline assembly differences between the certain values.
    /// Note: passing the parent function is necessary in order to properly
    /// generate the SyntaxDifference object.
    std::vector<SyntaxDifference> findAsmDifference(const CallInst *IL,
            const CallInst *IR) const;

    /// Detects a change from a function to a macro between two instructions.
    /// This is necessary because such a change isn't visible in C source.
    void findMacroFunctionDifference(const Instruction *L,
            const Instruction *R) const;
};

#endif //DIFFKEMP_SIMPLL_DIFFERENTIALFUNCTIONCOMPARATOR_H
