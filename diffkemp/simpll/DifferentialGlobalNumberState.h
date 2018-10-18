//===----- DifferentialGlobalNumberState.h - Numbering global symbols -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the DifferentialGlobalNumberState
/// class that extends the GlobalNumberState class from the LLVM's function
/// comparator for the specific purposes of SimpLL.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_DIFFERENTIALGLOBALNUMBERSTATE_H
#define DIFFKEMP_SIMPLL_DIFFERENTIALGLOBALNUMBERSTATE_H

#include <llvm/Transforms/Utils/FunctionComparator.h>
#include <llvm/IR/Module.h>

using namespace llvm;

class ModuleComparator;

/// Extension of GlobalNumberState.
/// Makes sure that matching globals in both compared modules get the same
/// number.
class DifferentialGlobalNumberState : public GlobalNumberState {
    // Mapping global values to numbers
    using ValueNumberMap = ValueMap<GlobalValue *, uint64_t>;
    ValueNumberMap GlobalNumbers;

    // Mapping strings to numbers
    StringMap<uint64_t> Strings;

    // Mapping APInts to numbers
    struct cmpConstants {
        bool operator()(const APInt &a, const APInt &b) const {
            return (a - b).isNegative();
        }
    };
    using IntegerMap = std::map<APInt, uint64_t, cmpConstants>;
    IntegerMap Constants{IntegerMap(cmpConstants())};

    std::vector<StringRef> PrintFunctionList = {"printk", "dev_warn", "dev_err",
        "_dev_info", "sprintf"};

    Module *First;
    Module *Second;

    u_int64_t nextNumber = 0;

    ModuleComparator *ModComparator;
  public:
    DifferentialGlobalNumberState(Module *first,
                                  Module *second,
                                  ModuleComparator *ModComparator) :
            First(first), Second(second), ModComparator(ModComparator) {
        // Add an entry for all print functions mapping them all to index 0
        for (StringRef Name : PrintFunctionList) {
            if (Function *F = first->getFunction(Name))
                GlobalNumbers.insert({F, 0});

            if (Function *F = second->getFunction(Name))
                GlobalNumbers.insert({F, 0});
        }

        // Index 0 is reserved for print functions
        ++nextNumber;
    }

    /// Get number of a global symbol. Corresponding symbols in compared modules
    /// get the same number.
    uint64_t getNumber(GlobalValue *value);

    /// Clear numbers mapping.
    void clear() {
        GlobalNumbers.clear();
    }
};

#endif //DIFFKEMP_SIMPLL_DIFFERENTIALGLOBALNUMBERSTATE_H
