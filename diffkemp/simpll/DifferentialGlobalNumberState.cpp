//===---- DifferentialGlobalNumberState.cpp - Numbering global symbols ----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains mapping of global symbols into numbers so that
/// corresponding symbols between modules get the same number.
///
//===----------------------------------------------------------------------===//

#include "DifferentialGlobalNumberState.h"
#include <llvm/IR/Module.h>

/// Get number for a global value.
/// Values with the same name are guaranteed to get the same number.
uint64_t DifferentialGlobalNumberState::getNumber(GlobalValue *value) {
    auto number = GlobalNumbers.find(value);
    u_int64_t result;
    if (number == GlobalNumbers.end()) {
        // Get new number for the global value
        result = nextNumber;
        GlobalNumbers.insert({value, nextNumber});

        // Try to find global value with the same name in the other module, if
        // it exists, assign it the same number
        auto otherModule = value->getParent() == First ? Second : First;
        GlobalValue *otherValue = otherModule->getNamedValue(
                value->getName());
        if (otherValue)
            GlobalNumbers.insert({otherValue, nextNumber});

        nextNumber++;
    } else {
        // If number for the global value exists, return it
        result = number->second;
    }

    return result;
}