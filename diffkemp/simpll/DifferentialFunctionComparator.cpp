//=== DifferentialFunctionComparator.cpp - Comparing functions for equality ==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementation of specific comparison functions used
/// to compare functions from different modules on equality.
///
//===----------------------------------------------------------------------===//

#include "DifferentialFunctionComparator.h"
#include "Config.h"
#include "SourceCodeUtils.h"
#include "passes/FieldAccessFunctionGenerator.h"
#include "passes/FunctionAbstractionsGenerator.h"
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <set>

// If a operand of a call instruction is detected to be generated from one of
// these macros, it should be always compared as equal.
std::set<std::string> ignoredMacroList = {
        "__COUNTER__", "__FILE__", "__LINE__", "__DATE__", "__TIME__"};

/// Compare GEPs. This code is copied from FunctionComparator::cmpGEPs since it
/// was not possible to simply call the original function.
/// Handles offset between matching GEP indices in the compared modules.
/// Uses data saved in StructFieldNames.
int DifferentialFunctionComparator::cmpGEPs(const GEPOperator *GEPL,
                                            const GEPOperator *GEPR) const {
    int OriginalResult = FunctionComparator::cmpGEPs(GEPL, GEPR);

    if (OriginalResult == 0)
        // The original function says the GEPs are equal - return the value
        return OriginalResult;

    if (!isa<StructType>(GEPL->getSourceElementType())
        || !isa<StructType>(GEPR->getSourceElementType()))
        // One of the types in not a structure - the original function is
        // sufficient for correct comparison
        return OriginalResult;

    if (getStructTypeName(dyn_cast<StructType>(GEPL->getSourceElementType()))
        != getStructTypeName(
                dyn_cast<StructType>(GEPR->getSourceElementType())))
        // Different structure names - the indices may be same by coincidence,
        // therefore index comparison can't be used
        return OriginalResult;

    unsigned int ASL = GEPL->getPointerAddressSpace();
    unsigned int ASR = GEPR->getPointerAddressSpace();

    if (int Res = cmpNumbers(ASL, ASR))
        return Res;

    if (int Res = cmpNumbers(GEPL->getNumIndices(), GEPR->getNumIndices()))
        return Res;

    if (GEPL->hasAllConstantIndices() && GEPR->hasAllConstantIndices()) {
        std::vector<Value *> IndicesL;
        std::vector<Value *> IndicesR;

        const GetElementPtrInst *GEPiL = dyn_cast<GetElementPtrInst>(GEPL);
        const GetElementPtrInst *GEPiR = dyn_cast<GetElementPtrInst>(GEPR);

        if (!GEPiL || !GEPiR)
            return OriginalResult;

        for (auto idxL = GEPL->idx_begin(), idxR = GEPR->idx_begin();
             idxL != GEPL->idx_end() && idxR != GEPR->idx_end();
             ++idxL, ++idxR) {
            auto ValueTypeL = GEPiL->getIndexedType(
                    GEPiL->getSourceElementType(), ArrayRef<Value *>(IndicesL));

            auto ValueTypeR = GEPiR->getIndexedType(
                    GEPiR->getSourceElementType(), ArrayRef<Value *>(IndicesR));

            auto NumericIndexL = dyn_cast<ConstantInt>(idxL->get())->getValue();
            auto NumericIndexR = dyn_cast<ConstantInt>(idxR->get())->getValue();

            if (!ValueTypeL->isStructTy() || !ValueTypeR->isStructTy()) {
                // If the indexed type is not a structure type, the indices have
                // to match in order for the instructions to be equivalent
                if (int Res = cmpValues(idxL->get(), idxR->get()))
                    return Res;

                IndicesL.push_back(*idxL);
                IndicesR.push_back(*idxR);

                continue;
            }

            // The indexed type is a structure type - compare the names of the
            // structure members from StructFieldNames.
            auto MemberNameL =
                    DI->StructFieldNames.find({dyn_cast<StructType>(ValueTypeL),
                                               NumericIndexL.getZExtValue()});

            auto MemberNameR =
                    DI->StructFieldNames.find({dyn_cast<StructType>(ValueTypeR),
                                               NumericIndexR.getZExtValue()});

            if (MemberNameL == DI->StructFieldNames.end()
                || MemberNameR == DI->StructFieldNames.end()
                || !MemberNameL->second.equals(MemberNameR->second))
                if (int Res = cmpValues(idxL->get(), idxR->get()))
                    return Res;

            IndicesL.push_back(*idxL);
            IndicesR.push_back(*idxR);
        }
    } else if (GEPL->getNumIndices() == 1 && GEPR->getNumIndices() == 1) {
        // If there is just a single (non-constant) index, it is an array
        // element access. We do not need to compare source element type since
        // members of the elements are not accessed here.
        // Just the index itself is compared.
        return cmpValues(GEPL->getOperand(1), GEPR->getOperand(1));
    } else
        // Indices can't be compared by name, because they are not constant
        return OriginalResult;

    return 0;
}

/// Ignore differences in attributes.
int DifferentialFunctionComparator::cmpAttrs(const AttributeList L,
                                             const AttributeList R) const {
    return 0;
}

/// Compare allocation instructions using separate cmpAllocs function in case
/// standard comparison returns something other than zero.
int DifferentialFunctionComparator::cmpOperations(
        const Instruction *L,
        const Instruction *R,
        bool &needToCmpOperands) const {
    int Result = FunctionComparator::cmpOperations(L, R, needToCmpOperands);

    // Check whether the instruction is a call instruction.
    if (isa<CallInst>(L) || isa<CallInst>(R)) {
        if (isa<CallInst>(L) && isa<CallInst>(R)) {
            const CallInst *CL = dyn_cast<CallInst>(L);
            const CallInst *CR = dyn_cast<CallInst>(R);

            Function *CalledL = CL->getCalledFunction();
            Function *CalledR = CR->getCalledFunction();
            if (CalledL && CalledR) {
                if (CalledL->getName() == CalledR->getName()) {
                    // Check whether both instructions call an alloc function.
                    if (isAllocFunction(*CalledL)) {
                        if (!cmpAllocs(CL, CR)) {
                            needToCmpOperands = false;
                            return 0;
                        }
                    }

                    if (CalledL->getIntrinsicID() == Intrinsic::memset
                        && CalledR->getIntrinsicID() == Intrinsic::memset) {
                        if (!cmpMemset(CL, CR)) {
                            needToCmpOperands = false;
                            return 0;
                        }
                    }

                    if (Result && controlFlowOnly
                        && abs(CL->getNumOperands() - CR->getNumOperands())
                                   == 1) {
                        needToCmpOperands = false;
                        return cmpCallsWithExtraArg(CL, CR);
                    }
                }
                if (Result) {
                    // If the call instructions are different (cmpOperations
                    // doesn't compare the called functions) or the called
                    // functions have different names, try inlining them.
                    // (except for case when one of the function is a SimpLL
                    // abstraction).
                    if (!isSimpllAbstractionDeclaration(CalledL)
                        && !isSimpllAbstractionDeclaration(CalledR))
                        ModComparator->tryInline = {CL, CR};

                    // Look for a macro-function difference.
                    findMacroFunctionDifference(L, R);
                }
            }
        } else {
            // If just one of the instructions is a call, it is possible that
            // some logic has been moved into a function. We'll try to inline
            // that function and compare again.
            if (isa<CallInst>(L)
                && !isSimpllAbstractionDeclaration(getCalledFunction(
                        dyn_cast<CallInst>(L)->getCalledValue())))
                ModComparator->tryInline = {dyn_cast<CallInst>(L), nullptr};
            else if (isa<CallInst>(R)
                     && !isSimpllAbstractionDeclaration(getCalledFunction(
                             dyn_cast<CallInst>(R)->getCalledValue())))
                ModComparator->tryInline = {nullptr, dyn_cast<CallInst>(R)};

            // Look for a macro-function difference.
            findMacroFunctionDifference(L, R);
        }
    }
    if (Result) {
        // Do not make difference between signed and unsigned for control flow
        // only
        if (controlFlowOnly && isa<ICmpInst>(L) && isa<ICmpInst>(R)) {
            auto *ICmpL = dyn_cast<ICmpInst>(L);
            auto *ICmpR = dyn_cast<ICmpInst>(R);
            if (ICmpL->getUnsignedPredicate()
                == ICmpR->getUnsignedPredicate()) {
                return 0;
            }
        }
        // Handle alloca of a structure type with changed layout
        if (isa<AllocaInst>(L) && isa<AllocaInst>(R)) {
            StructType *TypeL = dyn_cast<StructType>(
                    dyn_cast<AllocaInst>(L)->getAllocatedType());
            StructType *TypeR = dyn_cast<StructType>(
                    dyn_cast<AllocaInst>(R)->getAllocatedType());
            if (TypeL && TypeR
                && TypeL->getStructName() == TypeR->getStructName())
                return cmpNumbers(dyn_cast<AllocaInst>(L)->getAlignment(),
                                  dyn_cast<AllocaInst>(R)->getAlignment());
        }
    }

    if (Result) {
        auto macroDiffs = findMacroDifferences(L, R);
        ModComparator->DifferingObjects.insert(
                ModComparator->DifferingObjects.end(),
                macroDiffs.begin(),
                macroDiffs.end());
    }

    return Result;
}

/// Detects a change from a function to a macro between two instructions.
/// This is necessary because such a change isn't visible in C source.
void DifferentialFunctionComparator::findMacroFunctionDifference(
        const Instruction *L, const Instruction *R) const {
    // When trying to inline, it is possible that in one of the versions
    // a function correspond to a macro in the other. This is fixed by inlining
    // when the code is actually equal, but would lead to a difference being
    // reported in a function in which there is no apparent syntactic one. For
    // this reason a SyntaxDifference object is generated to report
    // the difference between the function and the macro.

    // First look whether this is the case described above.
    auto LineL = extractLineFromLocation(L->getDebugLoc());
    auto LineR = extractLineFromLocation(R->getDebugLoc());
    auto MacrosL = getAllMacrosAtLocation(L->getDebugLoc(), L->getModule());
    auto MacrosR = getAllMacrosAtLocation(R->getDebugLoc(), R->getModule());
    std::string NameL;
    std::string NameR;
    if (isa<CallInst>(L))
        NameL = getCalledFunction(dyn_cast<CallInst>(L)->getCalledValue())
                        ->getName();
    if (isa<CallInst>(R))
        NameR = getCalledFunction(dyn_cast<CallInst>(R)->getCalledValue())
                        ->getName();

    // Note: the line has to actually have been found for the comparison to make
    // sense.
    if ((LineL != "") && (LineR != "") && (LineL == LineR)
        && ((MacrosL.find(NameL) == MacrosL.end()
             && MacrosR.find(NameL) != MacrosR.end())
            || (MacrosL.find(NameR) != MacrosL.end()
                && MacrosR.find(NameR) == MacrosR.end()))) {
        std::string trueName;
        if ((MacrosL.find(NameL) == MacrosL.end()
             && MacrosR.find(NameL) != MacrosR.end())) {
            trueName = NameL;
            NameR = NameL + " (macro)";
            ModComparator->tryInline = {dyn_cast<CallInst>(L), nullptr};
        } else {
            trueName = NameR;
            NameL = NameR + " (macro)";
            ModComparator->tryInline = {nullptr, dyn_cast<CallInst>(R)};
        }

        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent() << "Writing function-macro "
                               << "syntactic difference\n");

        SyntaxDifference diff;
        diff.function = L->getFunction()->getName();
        diff.name = trueName;
        diff.BodyL = "[macro function difference]";
        diff.BodyR = "[macro function difference]";
        diff.StackL =
                CallStack{CallInfo{NameL,
                                   L->getDebugLoc()->getFile()->getFilename(),
                                   L->getDebugLoc()->getLine()}};
        diff.StackR =
                CallStack{CallInfo{NameR,
                                   R->getDebugLoc()->getFile()->getFilename(),
                                   R->getDebugLoc()->getLine()}};
        ModComparator->DifferingObjects.push_back(diff);
    }
}

/// Compare structure size with a constant.
/// @return 0 if equal, 1 otherwise.
int DifferentialFunctionComparator::cmpStructTypeSizeWithConstant(
        StructType *Type, const Value *Const) const {
    uint64_t ConstValue = dyn_cast<ConstantInt>(Const)->getZExtValue();
    return ConstValue != LayoutL.getTypeStoreSize(Type);
}

/// Handle comparing of memory allocation function in cases where the size
/// of the composite type is different.
int DifferentialFunctionComparator::cmpAllocs(const CallInst *CL,
                                              const CallInst *CR) const {
    // Look whether the sizes for allocation match. If yes, then return zero
    // (ignore flags).
    if (cmpValues(CL->op_begin()->get(), CR->op_begin()->get()) == 0) {
        return 0;
    }

    // The instruction is a call instrution calling the function kzalloc. Now
    // look whether the next instruction is a BitCastInst casting to a structure
    // type.
    if (!isa<BitCastInst>(CL->getNextNode())
        || !isa<BitCastInst>(CR->getNextNode()))
        return 1;

    // Check if kzalloc has constant size of the allocated memory
    if (!isa<ConstantInt>(CL->getOperand(0))
        || !isa<ConstantInt>(CR->getOperand(0)))
        return 1;

    // Get the allocated structure types
    StructType *STyL = getStructType(CL->getNextNode());
    StructType *STyR = getStructType(CR->getNextNode());

    // Return 0 (equality) if both allocated types are structs of the same name
    // and each struct has a size equal to the size of the allocated memory.
    return !STyL || !STyR
           || cmpStructTypeSizeWithConstant(STyL, CL->getOperand(0))
           || cmpStructTypeSizeWithConstant(STyR, CR->getOperand(0))
           || STyL->getStructName() != STyR->getStructName();
}

/// Check if the given operation can be ignored (it does not affect semantics)
/// for control flow only diffs.
bool DifferentialFunctionComparator::mayIgnore(const Instruction *Inst) const {
    if (controlFlowOnly)
        return isa<AllocaInst>(Inst) || Inst->isCast();
    else {
        if (isa<AllocaInst>(Inst))
            return true;
        if (auto Cast = dyn_cast<CastInst>(Inst)) {
            if (auto StrTy = dyn_cast<StructType>(Cast->getSrcTy())) {
                if (StrTy->hasName() && StrTy->getName().startswith("union"))
                    // Casting from a union type is safe to ignore, because in
                    // case it was generated by something different than a
                    // replacement of a simple type with a union, the
                    // comparator would detect a difference when the value is
                    // used, since it would have a different type.
                    return true;
            }

            if (Cast->getSrcTy()->isPointerTy()
                && Cast->getDestTy()->isPointerTy())
                // Pointer cast is also safe to ignore - in case there will be
                // an instruction affected by this (i.e. a store, load or GEP),
                // there would be an additional difference that will be detected
                // later.
                return true;

            if (Cast->getSrcTy()->isIntegerTy()
                && Cast->getDestTy()->isIntegerTy()) {
                // Integer-to-integer casts are safe to ignore only if two
                // conditions are met:
                // 1. The destination type is larger than the source type.
                // 2. No arithmetic is performed on the number.
                // Note: store instructions aren't a problem here, since the
                // types of the arithmetic instructions would be different if
                // there is arithmetic performed on the value after being
                // loaded back.
                auto IntTySrc = dyn_cast<IntegerType>(Cast->getSrcTy());
                auto IntTyDest = dyn_cast<IntegerType>(Cast->getDestTy());
                if (IntTySrc->getBitWidth() > IntTyDest->getBitWidth()) {
                    return false;
                }
                for (const User *U : Cast->users()) {
                    if (isa<BinaryOperator>(U)) {
                        return false;
                    }
                }
                return true;
            }
        }
        return false;
    }
}

bool mayIgnoreMacro(std::string macro) {
    return ignoredMacroList.find(macro) != ignoredMacroList.end();
}

/// Does additional comparisons based on the C source to determine whether two
/// call function arguments that may be compared as non-equal by LLVM are
/// actually semantically equal.
bool DifferentialFunctionComparator::cmpCallArgumentUsingCSource(
        const CallInst *CIL,
        const CallInst *CIR,
        Value *OpL,
        Value *OpR,
        unsigned i) const {
    // Try to prepare C source argument values to be used in operand
    // comparison.
    std::vector<std::string> CArgsL, CArgsR;
    const Function *CFL = getCalledFunction(CIL->getCalledValue());
    const Function *CFR = getCalledFunction(CIR->getCalledValue());
    const BasicBlock *BBL = CIL->getParent();
    const BasicBlock *BBR = CIR->getParent();

    // Use the appropriate C source call argument extraction function depending
    // on whether it is an inline assembly call or not.
    if (CFL->getName().startswith(SimpllInlineAsmPrefix))
        CArgsL = findInlineAssemblySourceArguments(
                CIL->getDebugLoc(),
                CIL->getModule(),
                ModComparator->AsmToStringMapL[CFL->getName()]);
    else
        CArgsL = findFunctionCallSourceArguments(
                CIL->getDebugLoc(), CIL->getModule(), CFL->getName());

    if (CFR->getName().startswith(SimpllInlineAsmPrefix))
        CArgsR = findInlineAssemblySourceArguments(
                CIR->getDebugLoc(),
                CIR->getModule(),
                ModComparator->AsmToStringMapR[CFR->getName()]);
    else
        CArgsR = findFunctionCallSourceArguments(
                CIR->getDebugLoc(), CIR->getModule(), CFL->getName());

    if ((CArgsL.size() > i) && (CArgsR.size() > i)) {
        if (mayIgnoreMacro(CArgsL[i]) && mayIgnoreMacro(CArgsR[i])
            && (CArgsL[i] == CArgsR[i])) {
            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                            dbgs() << getDebugIndent()
                                   << "Comparing integers as equal because of "
                                   << "correspondence to an ignored macro\n");
            return 0;
        }

        if (StringRef(CArgsL[i]).startswith("sizeof")
            && StringRef(CArgsR[i]).startswith("sizeof")
            && isa<ConstantInt>(OpL) && isa<ConstantInt>(OpR)) {
            // Both arguments are sizeofs; look whether they correspond to
            // a changed size of the same structure.
            int IntL = dyn_cast<ConstantInt>(OpL)->getZExtValue();
            int IntR = dyn_cast<ConstantInt>(OpR)->getZExtValue();
            auto SizeL = ModComparator->StructSizeMapL.find(IntL);
            auto SizeR = ModComparator->StructSizeMapR.find(IntR);

            // Extract the identifiers inside sizeofs
            auto IdL = getSubstringToMatchingBracket(CArgsL[i], 6);
            auto IdR = getSubstringToMatchingBracket(CArgsR[i], 6);
            IdL = IdL.substr(1, IdL.size() - 2);
            IdR = IdR.substr(1, IdR.size() - 2);

            auto TyIdL = getCSourceIdentifierType(
                    IdL, BBL->getParent(), DI->LocalVariableMapL);
            auto TyIdR = getCSourceIdentifierType(
                    IdR, BBR->getParent(), DI->LocalVariableMapR);

            // If the types are structure types, prepare their names for
            // comparison. (Use different generic names for non-structure
            // types.)
            // Note: since the sizeof is different the types would likely be
            // also compared as different using cmpTypes.
            auto TyIdLName =
                    TyIdL->isStructTy()
                            ? dyn_cast<StructType>(TyIdL)->getStructName()
                            : "Type 1";
            auto TyIdRName =
                    TyIdR->isStructTy()
                            ? dyn_cast<StructType>(TyIdR)->getStructName()
                            : "Type 2";

            // In case both values belong to the sizes of the same structure or
            // the original types were found and their names (in the case of
            // structure types) are the same, compare the sizeof as equal.
            if ((SizeL != ModComparator->StructSizeMapL.end()
                 && SizeR != ModComparator->StructSizeMapR.end()
                 && SizeL->second == SizeR->second)
                || (TyIdL != nullptr && TyIdR != nullptr
                    && (!cmpTypes(TyIdL, TyIdR) || TyIdLName == TyIdRName))) {
                DEBUG_WITH_TYPE(
                        DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "Comparing integers as equal because of"
                               << "correspondence to structure type sizes\n");
                return 0;
            }
        }
    }

    return 1;
}

/// Detect cast instructions and ignore them when comparing the control flow
/// only.
/// Note: this function was copied from FunctionComparator.
int DifferentialFunctionComparator::cmpBasicBlocks(
        const BasicBlock *BBL, const BasicBlock *BBR) const {
    BasicBlock::const_iterator InstL = BBL->begin(), InstLE = BBL->end();
    BasicBlock::const_iterator InstR = BBR->begin(), InstRE = BBR->end();

    while (InstL != InstLE && InstR != InstRE) {
        bool needToCmpOperands = true;

        if (int Res = cmpOperations(&*InstL, &*InstR, needToCmpOperands)) {
            // Some operations not affecting semantics and control flow may be
            // ignored (currently allocas and casts). This may help to handle
            // some small changes that do not affect semantics (it is also
            // useful in combination with function inlining).
            if (mayIgnore(&*InstL) || mayIgnore(&*InstR)) {
                // Reset serial counters
                sn_mapL.erase(&*InstL);
                sn_mapR.erase(&*InstR);
                // One of the compared operations will be skipped and the
                // comparison will be repeated.
                if (mayIgnore(&*InstL))
                    InstL++;
                else
                    InstR++;
                continue;
            }
            return Res;
        }
        if (needToCmpOperands) {
            assert(InstL->getNumOperands() == InstR->getNumOperands());

            for (unsigned i = 0, e = InstL->getNumOperands(); i != e; ++i) {
                Value *OpL = InstL->getOperand(i);
                Value *OpR = InstR->getOperand(i);

                if (int Res = cmpValues(OpL, OpR)) {
                    if (isa<CallInst>(&*InstL) && isa<CallInst>(&*InstR)) {
                        Res = cmpCallArgumentUsingCSource(
                                dyn_cast<CallInst>(&*InstL),
                                dyn_cast<CallInst>(&*InstR),
                                OpL,
                                OpR,
                                i);
                    }

                    if (Res) {
                        // Try to find macros that could be causing the
                        // difference
                        auto macroDiffs =
                                findMacroDifferences(&*InstL, &*InstR);

                        ModComparator->DifferingObjects.insert(
                                ModComparator->DifferingObjects.end(),
                                macroDiffs.begin(),
                                macroDiffs.end());

                        // Try to find assembly functions causing the difference
                        if (isa<CallInst>(&*InstL) && isa<CallInst>(&*InstR)
                            && showAsmDiff) {
                            auto asmDiffs = findAsmDifference(
                                    dyn_cast<CallInst>(&*InstL),
                                    dyn_cast<CallInst>(&*InstR));
                            ModComparator->DifferingObjects.insert(
                                    ModComparator->DifferingObjects.end(),
                                    asmDiffs.begin(),
                                    asmDiffs.end());
                        }

                        if (isa<CallInst>(&*InstL) && isa<CallInst>(&*InstR)) {
                            // If the instructions are call instructions, try
                            // to inline the functions.
                            auto CL = dyn_cast<CallInst>(&*InstL);
                            auto CR = dyn_cast<CallInst>(&*InstR);
                            auto CalledL =
                                    getCalledFunction(CL->getCalledValue());
                            auto CalledR =
                                    getCalledFunction(CR->getCalledValue());

                            if (!isSimpllAbstractionDeclaration(CalledL)
                                && !isSimpllAbstractionDeclaration(CalledR))
                                ModComparator->tryInline = {CL, CR};

                            // Look for a macro-function difference.
                            findMacroFunctionDifference(&*InstL, &*InstR);
                        }

                        return Res;
                    }
                }
                // cmpValues should ensure this is true.
                assert(cmpTypes(OpL->getType(), OpR->getType()) == 0);
            }
        }

        ++InstL;
        ++InstR;
    }

    if (InstL != InstLE && InstR == InstRE)
        return 1;
    if (InstL == InstLE && InstR != InstRE)
        return -1;
    return 0;
}

/// Looks for inline assembly differences between the call instructions.
std::vector<SyntaxDifference> DifferentialFunctionComparator::findAsmDifference(
        const CallInst *IL, const CallInst *IR) const {
    auto FunL = getCalledFunction(IL->getCalledValue());
    auto FunR = getCalledFunction(IR->getCalledValue());
    auto ParentL = IL->getFunction();
    auto ParentR = IR->getFunction();

    if (!FunL || !FunR)
        // Both values have to be functions
        return {};

    if (!FunL->getName().startswith(SimpllInlineAsmPrefix)
        || !FunR->getName().startswith(SimpllInlineAsmPrefix))
        // Both functions have to be assembly abstractions
        return {};

    StringRef AsmL = ModComparator->AsmToStringMapL[FunL->getName()];
    StringRef AsmR = ModComparator->AsmToStringMapR[FunR->getName()];
    if (AsmL == AsmR)
        // The difference is somewhere else
        return {};

    // Iterate over inline asm operands and generate a C-like identifier for
    // every one of them.
    // Note: because this function's main purpose is to show inline assembly
    // differences in cases when the original macro containing them cannot be
    // found by SourceCodeUtils, the original arguments in the C source code
    // also cannot be localed, therefore the C-like identifier is used instead.
    std::string argumentNamesL, argumentNamesR;
    for (auto T : {std::make_tuple(IL, &argumentNamesL),
                   std::make_tuple(IR, &argumentNamesR)}) {
        // The identifier generation is done separately for the left and right
        // call instruction; vector of tuples and pointers are used in order to
        // re-use the code for both.
        const CallInst *I = std::get<0>(T);
        std::string *argumentNames = std::get<1>(T);

        for (int i = 0; i < I->getNumArgOperands(); i++) {
            const Value *Op = I->getArgOperand(i);
            std::string OpName = getIdentifierForValue(
                    Op, DI->StructFieldNames, I->getFunction());

            if (*argumentNames == "")
                *argumentNames += OpName;
            else
                *argumentNames += ", " + OpName;
        }
    }

    // Create difference object.
    // Note: the call stack is left empty here, it will be added in reportOutput
    SyntaxDifference diff;
    diff.BodyL = AsmL.str() + " (args: " + argumentNamesL + ")";
    diff.BodyR = AsmR.str() + " (args: " + argumentNamesR + ")";
    diff.StackL = CallStack();
    diff.StackL.push_back(CallInfo{"(generated assembly code)",
                                   ParentL->getSubprogram()->getFilename(),
                                   ParentL->getSubprogram()->getLine()});
    diff.StackR = CallStack();
    diff.StackR.push_back(CallInfo{"(generated assembly code)",
                                   ParentR->getSubprogram()->getFilename(),
                                   ParentR->getSubprogram()->getLine()});
    diff.function = ParentL->getName();
    diff.name = "assembly code "
                + std::to_string(++ModComparator->asmDifferenceCounter);

    return {diff};
}

/// Implement comparison of global values that does not use a
/// GlobalNumberState object, since that approach does not fit the use case
/// of comparing functions in two different modules.
int DifferentialFunctionComparator::cmpGlobalValues(GlobalValue *L,
                                                    GlobalValue *R) const {
    auto GVarL = dyn_cast<GlobalVariable>(L);
    auto GVarR = dyn_cast<GlobalVariable>(R);

    if (GVarL && GVarR && GVarL->hasInitializer() && GVarR->hasInitializer()
        && GVarL->isConstant() && GVarR->isConstant()) {
        // Constant global variables are compared using their initializers.
        return cmpConstants(GVarL->getInitializer(), GVarR->getInitializer());
    } else if (L->hasName() && R->hasName()) {
        // Both values are named, compare them by names
        auto NameL = L->getName();
        auto NameR = R->getName();

        // Remove number suffixes
        if (hasSuffix(NameL))
            NameL = NameL.substr(0, NameL.find_last_of("."));
        if (hasSuffix(NameR))
            NameR = NameR.substr(0, NameR.find_last_of("."));
        if (NameL == NameR
            || (isPrintFunction(NameL) && isPrintFunction(NameR))) {
            if (isa<Function>(L) && isa<Function>(R)) {
                // Functions compared as being the same have to be also compared
                // by ModuleComparator.
                auto FunL = dyn_cast<Function>(L);
                auto FunR = dyn_cast<Function>(R);

                // Do not compare SimpLL abstractions.
                if (!isSimpllAbstraction(FunL) && !isSimpllAbstraction(FunR)
                    && (ModComparator->ComparedFuns.find({FunL, FunR})
                        == ModComparator->ComparedFuns.end())
                    && (!isPrintFunction(L->getName())
                        && !isPrintFunction(R->getName()))) {
                    ModComparator->compareFunctions(FunL, FunR);
                }

                if (FunL->getName().startswith(SimpllFieldAccessFunName)
                    && FunR->getName().startswith(SimpllFieldAccessFunName)) {
                    // Compare field access abstractions using a special
                    // method.
                    return cmpFieldAccess(FunL, FunR);
                } else if (FunL->getName().startswith(SimpllInlineAsmPrefix)
                           && FunR->getName().startswith(
                                   SimpllInlineAsmPrefix)) {
                    return ModComparator->AsmToStringMapL[FunL->getName()]
                           != ModComparator->AsmToStringMapR[FunR->getName()];
                }
            }
            return 0;
        } else
            return 1;
    } else
        return L != R;
}

// Takes all GEPs in a basic block and computes the sum of their offsets if
// constant (if not, it returns false).
bool DifferentialFunctionComparator::accumulateAllOffsets(
        const BasicBlock &BB, uint64_t &Offset) const {
    for (auto &Inst : BB) {
        if (auto GEP = dyn_cast<GetElementPtrInst>(&Inst)) {
            APInt Offset;
            if (!GEP->accumulateConstantOffset(BB.getModule()->getDataLayout(),
                                               Offset))
                return false;
            Offset += Offset.getZExtValue();
        }
    }
    return true;
}

/// Specific comparing of structure field access.
int DifferentialFunctionComparator::cmpFieldAccess(const Function *L,
                                                   const Function *R) const {
    // First compute the complete offset of all GEPs in both functions.
    // It is possible that although the process contains unpacking several
    // anonymous unions and structs, the offset stays the same, which means that
    // without a doubt this is semantically equal (and probably would, in fact,
    // be also equal in machine code).
    uint64_t OffsetL = 0, OffsetR = 0;

    if (!accumulateAllOffsets(L->front(), OffsetL)
        || !accumulateAllOffsets(L->front(), OffsetR))
        return 1;

    if (OffsetL == OffsetR)
        // If the complete offsets are the same, the field accesses are
        // semantically also the same (the source and target type are compared
        // in cmpOperations and cmpBasicBlocks as the instruction type and
        // operand type).
        return 0;

    // If all of the specific comparisons failed, report non-equal, which will
    // lead to inling of the abstractions and comparing the instuction the
    // usual way.
    return 1;
}

/// Handle values generated from macros and enums whose value changed.
/// The new values are pre-computed by DebugInfo.
/// Also handles comparing in case at least one of the values is a cast -
/// when comparing the control flow only, it compares the original value instead
/// of the cast.
int DifferentialFunctionComparator::cmpValues(const Value *L,
                                              const Value *R) const {
    // Detect casts and use the original value instead when comparing the
    // control flow only.
    const CastInst *CIL = dyn_cast<CastInst>(L);
    const CastInst *CIR = dyn_cast<CastInst>(R);

    if (CIL && CIR && mayIgnore(CIL) && mayIgnore(CIR)) {
        // Both instruction are casts - compare the original values before
        // the cast
        return cmpValues(CIL->getOperand(0), CIR->getOperand(0));
    } else if (CIL && mayIgnore(CIL)) {
        // The left value is a cast - use the original value of it in the
        // comparison
        return cmpValues(CIL->getOperand(0), R);
    } else if (CIR && mayIgnore(CIR)) {
        // The right value is a cast - use the original value of it in the
        // comparison
        return cmpValues(L, CIR->getOperand(0));
    }

    int result = FunctionComparator::cmpValues(L, R);
    if (result) {
        if (isa<ConstantExpr>(L)) {
            // Try comparing constant expressions as instructions.
            auto InstL = getConstExprAsInstruction(dyn_cast<ConstantExpr>(L));
            return cmpValues(InstL, R);
        } else if (isa<ConstantExpr>(R)) {
            // Try comparing constant expressions as instructions.
            auto InstR = getConstExprAsInstruction(dyn_cast<ConstantExpr>(R));
            return cmpValues(L, InstR);
        } else if (isa<Constant>(L) && isa<Constant>(R)) {
            auto *ConstantL = dyn_cast<Constant>(L);
            auto *ConstantR = dyn_cast<Constant>(R);
            auto MacroMapping = DI->MacroConstantMap.find(ConstantL);
            if (MacroMapping != DI->MacroConstantMap.end()
                && MacroMapping->second == valueAsString(ConstantR))
                return 0;
        } else if (isa<BasicBlock>(L) && isa<BasicBlock>(R)) {
            // In case functions have different numbers of BBs, they may be
            // compared as unequal here. However, this can be caused by moving
            // part of the functionality into a function and hence we'll
            // treat the BBs as equal here to continue comparing and maybe
            // try inlining.
            // We also need to remove a BB that was newly inserted in cmpValues
            // since the serial maps would not be synchronized otherwise.
            if (sn_mapL.size() != sn_mapR.size()) {
                if (sn_mapL[L] == (sn_mapL.size() - 1))
                    sn_mapL.erase(L);
                if (sn_mapR[R] == (sn_mapR.size() - 1))
                    sn_mapR.erase(R);
            }
            return 0;
        }
    }
    return result;
}

/// Specific comparing of constants. If one of them (or both) is a cast
/// constant expression, compare its operand.
int DifferentialFunctionComparator::cmpConstants(const Constant *L,
                                                 const Constant *R) const {
    int Result = FunctionComparator::cmpConstants(L, R);

    if (Result == 0)
        return Result;

    if (controlFlowOnly) {
        // Look whether the constants is a cast ConstantExpr
        const ConstantExpr *UEL = dyn_cast<ConstantExpr>(L);
        const ConstantExpr *UER = dyn_cast<ConstantExpr>(R);

        // We want to compare only casts by their operands
        if (UEL && !UEL->isCast())
            UEL = nullptr;
        if (UER && !UER->isCast())
            UER = nullptr;

        if (UEL && UER) {
            return cmpConstants(UEL->getOperand(0), UER->getOperand(0));
        } else if (UEL) {
            return cmpConstants(UEL->getOperand(0), R);
        } else if (UER) {
            return cmpConstants(L, UER->getOperand(0));
        }
    }

    return Result;
}

int DifferentialFunctionComparator::cmpCallsWithExtraArg(
        const CallInst *CL, const CallInst *CR) const {
    // Distinguish which call has more parameters
    const CallInst *CallExtraArg;
    const CallInst *CallOther;
    if (CL->getNumOperands() > CR->getNumOperands()) {
        CallExtraArg = CL;
        CallOther = CR;
    } else {
        CallExtraArg = CR;
        CallOther = CL;
    }

    // The last extra argument must be 0 (false) or NULL
    auto *LastOp = CallExtraArg->getOperand(CallExtraArg->getNumOperands() - 2);
    if (auto ConstLastOp = dyn_cast<Constant>(LastOp)) {
        if (!(ConstLastOp->isNullValue() || ConstLastOp->isZeroValue()))
            return 1;

        // Compare function return types (types of the call instructions)
        if (int Res = cmpTypes(CallExtraArg->getType(), CallOther->getType()))
            return Res;

        // For each argument (except the extra one), compare its type and value.
        // Last argument is not compared since it is the called function.
        for (unsigned i = 0, e = CallOther->getNumOperands() - 1; i != e; ++i) {
            auto Arg1 = CallExtraArg->getOperand(i);
            auto Arg2 = CallOther->getOperand(i);
            if (int Res = cmpTypes(Arg1->getType(), Arg2->getType()))
                return Res;
            if (int Res = cmpValues(Arg1, Arg2))
                return Res;
        }
        return 0;
    }
    return 1;
}

/// Compares array types with equivalent element types as equal when
/// comparing the control flow only.
int DifferentialFunctionComparator::cmpTypes(Type *L, Type *R) const {
    // Compare union as equal to another type in case it is at least of the same
    // size.
    // Note: a union type is represented in Clang-generated LLVM IR by a
    // structure type.
    if (L->isStructTy() || R->isStructTy()) {
        Type *Ty;
        StructType *StrTy;
        const DataLayout *TyLayout, *StrTyLayout;
        if (L->isStructTy()) {
            StrTy = dyn_cast<StructType>(L);
            Ty = R;
            StrTyLayout = &LayoutL;
            TyLayout = &LayoutR;
        } else {
            StrTy = dyn_cast<StructType>(R);
            Ty = L;
            StrTyLayout = &LayoutR;
            TyLayout = &LayoutL;
        }

        if (StrTy->getStructName().startswith("union")
            && (StrTyLayout->getTypeAllocSize(StrTy)
                >= TyLayout->getTypeAllocSize(Ty))) {
            return 0;
        }
    }

    // Compare integer types (except the boolean type) as the same when
    // comparing the control flow only.
    if (L->isIntegerTy() && R->isIntegerTy() && controlFlowOnly) {
        if (L->getIntegerBitWidth() == 1 || R->getIntegerBitWidth() == 1)
            return !(L->getIntegerBitWidth() == R->getIntegerBitWidth());
        return 0;
    }

    if (!L->isArrayTy() || !R->isArrayTy() || !controlFlowOnly)
        return FunctionComparator::cmpTypes(L, R);

    ArrayType *AL = dyn_cast<ArrayType>(L);
    ArrayType *AR = dyn_cast<ArrayType>(R);

    return cmpTypes(AL->getElementType(), AR->getElementType());
}

/// Do not compare bitwidth when comparing the control flow only.
int DifferentialFunctionComparator::cmpAPInts(const APInt &L,
                                              const APInt &R) const {
    int Result = FunctionComparator::cmpAPInts(L, R);
    if (!controlFlowOnly || !Result) {
        return Result;
    } else {
        // The function ugt uses APInt::compare, which can compare only integers
        // of the same bitwidth. When we want to also compare integers of
        // different bitwidth, a different approach has to be used.
        return cmpNumbers(L.getZExtValue(), R.getZExtValue());
    }

    return 0;
}

/// Comparison of memset functions.
/// Handles situation when memset sets the memory occupied by a structure, but
/// the structure size changed.
int DifferentialFunctionComparator::cmpMemset(const CallInst *CL,
                                              const CallInst *CR) const {
    // Compare all except the third operand (size to set).
    for (unsigned i = 0; i < CL->getNumArgOperands(); ++i) {
        if (i == 2)
            continue;
        if (int Res = cmpValues(CL->getArgOperand(i), CR->getArgOperand(i)))
            return Res;
    }

    if (!cmpValues(CL->getArgOperand(2), CR->getArgOperand(2)))
        return 0;

    // Get the struct types of memset destinations.
    StructType *STyL = getStructType(CL->getOperand(0));
    StructType *STyR = getStructType(CR->getOperand(0));

    // Return 0 (equality) if both memory destinations are structs of the same
    // name and each memset size is equal to the corresponding struct size.
    return !STyL || !STyR
           || cmpStructTypeSizeWithConstant(STyL, CL->getOperand(2))
           || cmpStructTypeSizeWithConstant(STyR, CR->getOperand(2))
           || STyL->getStructName() != STyR->getStructName();
}
