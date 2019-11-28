//===-- StructureSizeAnalysis.cpp - extraction of struct type debug info --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the StructureDebugInfoAnalysis pass.
///
//===----------------------------------------------------------------------===//

#include "StructureDebugInfoAnalysis.h"
#include <llvm/IR/DebugInfoMetadata.h>

AnalysisKey StructureDebugInfoAnalysis::Key;

StructureDebugInfoAnalysis::Result StructureDebugInfoAnalysis::run(
        Module &Mod, AnalysisManager<Module, Function *> &mam, Function *Main) {
    Result Res;
    for (DICompileUnit *CU : Mod.debug_compile_units()) {
        // Use a DFS algorithm to get to all DI nodes for structure types.
        std::vector<DIType *> Stack;
        // Add retained types.
        DIScopeArray Nodes = CU->getRetainedTypes();
        for (DIScope *DNd : Nodes) {
            if (auto DTy = dyn_cast<DIType>(DNd))
                Stack.push_back(DTy);
        }
        // Add global variable types.
        DIGlobalVariableExpressionArray GExprs = CU->getGlobalVariables();
        for (DIGlobalVariableExpression *GExpr : GExprs) {
            DIGlobalVariable *GVar = GExpr->getVariable();
#if LLVM_VERSION_MAJOR < 9
            Stack.push_back(GVar->getType().resolve());
#else
            Stack.push_back(GVar->getType());
#endif
        }
        // The actual DFS.
        // Note: since the type graph apparently is not a tree, a set to record
        // already processed values has to be used.
        std::set<DIType *> processedVals;
        while (!Stack.empty()) {
            DIType *DTy = Stack.back();
            Stack.pop_back();
            if (processedVals.find(DTy) != processedVals.end())
                continue;
            processedVals.insert(DTy);
            if (auto DDTy = dyn_cast<DIDerivedType>(DTy)) {
#if LLVM_VERSION_MAJOR < 9
                if (DDTy->getBaseType().resolve())
                    Stack.push_back(DDTy->getBaseType().resolve());
#else
                if (DDTy->getBaseType())
                    Stack.push_back(DDTy->getBaseType());
#endif
            } else if (auto DCTy = dyn_cast<DICompositeType>(DTy)) {
                if (DCTy->getTag() == dwarf::DW_TAG_structure_type
                    && DCTy->getName() != "") {
                    // The type is a structure type, add entry to map.
                    Res[DCTy->getName()] = DCTy;
                }
                // Go through all types inside the composite type.
                DINodeArray Elems = DCTy->getElements();
                for (DINode *DNd : Elems) {
                    if (auto DTy2 = dyn_cast<DIType>(DNd)) {
                        Stack.push_back(DTy2);
                    }
                }
            }
        }
    }
    return Res;
}
