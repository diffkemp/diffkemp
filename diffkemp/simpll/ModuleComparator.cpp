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
#include <llvm/Support/raw_ostream.h>

/// Syntactical comparison of functions.
/// Function declarations are equal if they have the same name.
/// Functions with body are compared using custom FunctionComparator that
/// is designed for comparing functions between different modules.
void ModuleComparator::compareFunctions(Function *FirstFun,
                                        Function *SecondFun) {
#ifdef DEBUG
    errs() << "Comparing " << FirstFun->getName() << " and "
           << SecondFun->getName() << "\n";
#endif
    ComparedFuns.emplace(std::make_pair(FirstFun, SecondFun), Result::UNKNOWN);

    // Comparing function declarations (function without bodies).
    if (FirstFun->isDeclaration() || SecondFun->isDeclaration()) {
        if (FirstFun->isDeclaration() && SecondFun->isDeclaration()
                && FirstFun->getName() == SecondFun->getName()) {
            ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
        } else {
            ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
        }
        return;
    }

    // Comparing functions with bodies using custom FunctionComparator.
    DifferentialFunctionComparator fComp(FirstFun, SecondFun, &GS, DI);
    if (fComp.compare() == 0) {
#ifdef DEBUG
        errs() << "Function " << FirstFun->getName()
               << " is same in both modules\n";
#endif
        FirstFun->deleteBody();
        SecondFun->deleteBody();
        deleteAliasToFun(First, FirstFun);
        deleteAliasToFun(Second, SecondFun);

        ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
    } else {
        ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
    }
}
