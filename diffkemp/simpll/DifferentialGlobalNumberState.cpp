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

        // Try to find global value with the same name in the other module, if
        // it exists, assign it the same number
        auto otherModule = value->getParent() == First ? Second : First;
        auto GA = dyn_cast<GlobalVariable>(value);

        // If the value is a GlobalVariable, has the attribute unnamed_addr and
        // the initializer is either a ConstantDataSequential or a ConstantInt,
        // use a map for comparison by value instead of name
        if (GA && GA->hasGlobalUnnamedAddr() && GA->hasInitializer() &&
            (dyn_cast<ConstantDataSequential>(GA->getInitializer()) ||
             dyn_cast<ConstantInt>(GA->getInitializer()))) {
            // Value is a constant that can be compared by value
            Constant *I = GA->getInitializer();

            if (ConstantDataSequential *CDS =
                dyn_cast<ConstantDataSequential>(I)) {
                // Value is a string - look it up in the string map
                auto possibleResult = Strings.find(
                    CDS->getAsString());

                if(possibleResult != Strings.end()) {
                    // If the string is in the map, set the result to it
                    result = (*possibleResult).getValue();
                } else {
                    // If it isn't, assign it the next number and insert it to
                    // both GlobalNumbers and the string map
                    Strings.insert({CDS->getAsString(), nextNumber});
                    GlobalNumbers.insert({value, nextNumber});

                    result = nextNumber;
                    nextNumber++;
                }
            } else if (ConstantInt *CI =
                    dyn_cast<ConstantInt>(I)) {
                auto possibleResult = Constants.find(CI->getValue());

                if (possibleResult != Constants.end()) {
                    // If the APInt is in the map, set the result to it
                    result = (*possibleResult).second;
                } else {
                    // If it isn't, assign it the next number and insert it to
                    // both GlobalNumbers and the APInt map
                    Constants.insert({CI->getValue(), nextNumber});
                    GlobalNumbers.insert({value, nextNumber});

                    result = nextNumber;
                    nextNumber++;
                }
            }
        } else {
            GlobalNumbers.insert({value, nextNumber});

            GlobalValue *otherValue = otherModule->getNamedValue(
                    value->getName());
            if (otherValue)
                GlobalNumbers.insert({otherValue, nextNumber});

            nextNumber++;
        }
    } else {
        // If number for the global value exists, return it
        result = number->second;
    }

    return result;
}
