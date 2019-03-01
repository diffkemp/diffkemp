//===----------- ModuleComparator.cpp - Numbering global symbols ----------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definitions of methods of the ModuleComparator class that
/// can be used for syntactical comparison of two LLVM modules.
///
//===----------------------------------------------------------------------===//

#include "ModuleComparator.h"
#include "DifferentialFunctionComparator.h"
#include "Utils.h"
#include "Config.h"
#include <llvm/Support/raw_ostream.h>

/// Syntactical comparison of functions.
/// Function declarations are equal if they have the same name.
/// Functions with body are compared using custom FunctionComparator that
/// is designed for comparing functions between different modules.
void ModuleComparator::compareFunctions(Function *FirstFun,
                                        Function *SecondFun) {
    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                    dbgs() << "Comparing " << FirstFun->getName() << " and "
                           << SecondFun->getName() << "\n");
    ComparedFuns.emplace(std::make_pair(FirstFun, SecondFun), Result::UNKNOWN);

    // Comparing function declarations (function without bodies).
    if (FirstFun->isDeclaration() || SecondFun->isDeclaration()) {
        if (controlFlowOnly) {
            // If checking control flow only, it suffices that one of the
            // functions is a declaration to treat them equal.
            if (FirstFun->getName() == SecondFun->getName())
                ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
            else
                ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
        }
        else {
            if (FirstFun->isDeclaration() && SecondFun->isDeclaration()
                    && FirstFun->getName() == SecondFun->getName())
                ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
            else
                ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
        }
        return;
    }

    // Comparing functions with bodies using custom FunctionComparator.
    DifferentialFunctionComparator fComp(FirstFun, SecondFun, controlFlowOnly,
                                         &GS, DI, this);
    if (fComp.compare() == 0) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << "Function " << FirstFun->getName()
                               << " is same in both modules\n");
        ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
    } else {
        ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
        while (tryInline) {
            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                            dbgs() << "Try to inline " << tryInline->getName()
                                   << "\n");
            // Try to inline the problematic function
            std::string nameToInline = tryInline->getName();
            Function *inlineFirst = First.getFunction(nameToInline);
            Function *inlineSecond = Second.getFunction(nameToInline);
            // Do not inline if the function to be inlined has already been
            // compared and is non-equal. In such case, inlining could remove
            // the function and we could not get information about it later.
            auto compared = ComparedFuns.find({inlineFirst, inlineSecond});
            if (compared == ComparedFuns.end()
                    || compared->second == Result::EQUAL) {
                // Inline the function in the appropriate module and simplify
                // the other function (because inlining does the same
                // simplification to the caller of the inlined function).
                if (inlineFirst) {
                    inlineFunction(First, inlineFirst);
                    simplifyFunction(SecondFun);
                }
                if (inlineSecond) {
                    inlineFunction(Second, inlineSecond);
                    simplifyFunction(FirstFun);
                }
                // Reset the function diff result
                ComparedFuns.at({FirstFun, SecondFun}) = Result::UNKNOWN;
                tryInline = nullptr;
                // Re-run the comparison
                DifferentialFunctionComparator fCompSecond(FirstFun, SecondFun,
                                                           controlFlowOnly,
                                                           &GS, DI, this);
                if (fCompSecond.compare() == 0) {
                    ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
                } else {
                    ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
                }
            } else {
                tryInline = nullptr;
            }
        }
    }
}
