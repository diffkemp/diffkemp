//===------ UnifyGlobalsNumberingPass.h - Unify numbering of globals ------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the UnifyGlobalsNumberingPass pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_UNIFYGLOBALSNUMBERINGPASS_H
#define DIFFKEMP_SIMPLL_UNIFYGLOBALSNUMBERINGPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

/// This pass goes through global values with names ending in a number in both
/// modules and unifies them when possible.
class UnifyGlobalsNumberingPass
        : public PassInfoMixin<UnifyGlobalsNumberingPass> {
  public:
    PreservedAnalyses run(Module &Mod, AnalysisManager<Module, Function *> &mam,
                          Function *Main, Module *ModOther);
  private:
    // Ensure that the numbering of static local variables is consistent in
    // cases when there are more of them with the same name.
    void fixStaticVariablesNumbering(Module &Mod);
};

// Gets an instruction up in the user tree or nullptr in case it doesn't exist.
const Instruction *getUserInstruction(const User *U);

#endif //DIFFKEMP_SIMPLL_UNIFYGLOBALSNUMBERINGPASS_H
