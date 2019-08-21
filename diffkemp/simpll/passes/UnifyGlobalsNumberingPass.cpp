//==------ UnifyGlobalsNumberingPass.cpp - Unify numbering of globals ------==//
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

#include "CalledFunctionsAnalysis.h"
#include "UnifyGlobalsNumberingPass.h"
#include "Utils.h"
#include <Config.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>

PreservedAnalyses UnifyGlobalsNumberingPass::run(
        Module &Mod,
        AnalysisManager<Module, Function *> &mam,
        Function *Main,
        Module *ModOther) {
    // Currently this pass does only the unification for static variables.
    fixStaticVariablesNumbering(Mod);

    return PreservedAnalyses();
}

// Ensure that the numbering of static local variables is consistent in
// cases when there are more of them with the same name.
void UnifyGlobalsNumberingPass::fixStaticVariablesNumbering(Module &Mod) {
    // This is a map that groups together variables differing only in number
    // suffix along with the instruction numbers of their first users.
    std::unordered_map<std::string,
        std::vector<std::pair<GlobalVariable *, int>>> Map;

    // Iterate over all globals and for eligible ones, retrieve their
    // instruction number and save it to the map described above.
    for (GlobalValue &GV : Mod.globals()) {
        if (!GV.hasName() || !isa<GlobalVariable>(GV))
            // Static variables always have a name and are variables.
            continue;

        auto GVa = dyn_cast<GlobalVariable>(&GV);
        SmallVector<DIGlobalVariableExpression *, 10> DIStore;
        GVa->getDebugInfo(DIStore);

        if (DIStore.size() != 1)
            // Static variables should have exactly one node.
            continue;

        auto DIGV = DIStore[0]->getVariable();
        if (!isa<DISubprogram>(DIGV->getScope()))
            // Not defined inside function (not "local" in the C sense).
            continue;

        // At this point we can be sure that we are dealing with a static
        // local variable. Check whether it has a number suffix.
        int Offset = GV.getName().find(("." + DIGV->getName()).str()) +
                DIGV->getName().size() + 2;
        if (Offset > GV.getName().size())
            // There is no number suffix, no action needed.
            continue;

        // Now determine the number of the instruction where the value is.
        // Note: the variable should have at least one user.
        int minimum = INT32_MAX;
        for (auto U : GV.users()) {
            auto Instr = getUserInstruction(U);
            if (!Instr)
                continue;
            int n = 0;
            for (auto &BB : *(Instr->getFunction())) {
                for (auto &I : BB) {
                    if (&I == Instr && n < minimum) {
                        minimum = n;
                        goto end;
                    }
                    n++;
                }
            }
            end:;
        }
        if (minimum == INT32_MAX)
            continue;
        Map[GV.getName().substr(0, Offset - 1)].push_back({GVa, minimum});
    }

    // Go through all static variables in map and number them according to the
    // order of the numbers assigned in the previous step.
    for (auto Elem : Map) {
        std::sort(Elem.second.begin(), Elem.second.end());
        int counter = 0;
        for (auto Pair : Elem.second) {
            Pair.first->setName(Elem.first + "." + std::to_string(counter++));
        }
    }
}

// Gets an instruction up in the user tree or nullptr in case it doesn't exist.
const Instruction *getUserInstruction(const User *U) {
    if (isa<Instruction>(U))
        return dyn_cast<Instruction>(U);
    else {
        if (U->user_empty())
            return nullptr;
        return getUserInstruction(*U->user_begin());
    }
}
