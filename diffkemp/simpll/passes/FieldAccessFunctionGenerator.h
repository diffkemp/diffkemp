//== FieldAccessFunctionGenerator.h - Move field access blocks to function ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the FieldAccessFunctionGenerator pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_FIELDACCESSFUNCTIONGENERATOR_H
#define DIFFKEMP_SIMPLL_FIELDACCESSFUNCTIONGENERATOR_H

#include <llvm/IR/PassManager.h>

const std::string SimpllFieldAccessFunName = "simpll__fieldaccess";
const std::string SimpllFieldAccessMetadata = "fieldaccess";

using namespace llvm;

/// A pass that takes blocks implementing structure field access (i.e. GEPs,
/// casts) and creates a function for each one when possible (i.e. when the
/// instructions have the same debug location and there are more than two of
/// them; they also must not be accessed by any other instructions that those
/// in the block and they may not access any value outside of the block in order
/// not to break the code).
class FieldAccessFunctionGenerator
        : public PassInfoMixin<FieldAccessFunctionGenerator> {
  public:
    PreservedAnalyses run(Module &Mod, AnalysisManager<Module, Function *> &mam,
                          Function *Main, Module *ModOther);
  private:
    void processStack(const std::vector<Instruction *> &Stack, Module &Mod);
};

#endif //DIFFKEMP_SIMPLL_FIELDACCESSFUNCTIONGENERATOR_H
