//===------ DiffKempUtils.cpp - utility functions for use in DiffKemp -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementations of functions for looking up locations of
/// functions and global variables that affect module and sysctl parameters for
/// use in the generate phase of DiffKemp.
///
//===----------------------------------------------------------------------===//

#include "DiffKempUtils.h"
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>

/// Extract the name of the global variable representing a module parameter from
/// the structure describing it.
/// \param[in] ParamVar: LLVM expression containing the variable
/// \return Name of the variable
StringRef extractParamName(const Value *ParamVal) {
    if (auto GVar = dyn_cast<GlobalVariable>(ParamVal)) {
        StringRef ParamName = GVar->getName();

        if (ParamName.find("__param_arr") != StringRef::npos
            || ParamName.find("__param_string") != StringRef::npos) {
            // For array and string parameters, the actual variable is inside
            // another structure as its last element.
            auto Init = GVar->getInitializer();
            auto InitStr = dyn_cast<ConstantStruct>(Init);
            if (!InitStr)
                return "";
            auto VarExpr = InitStr->getOperand(InitStr->getNumOperands() - 1);

            return extractParamName(VarExpr);
        } else {
            return ParamName;
        }
    }

    if (auto CExpr = dyn_cast<ConstantExpr>(ParamVal)) {
        // Variable can be inside bitcast or getelementptr, in both cases it is
        // inside the first operand of the expression.
        return extractParamName(CExpr->getOperand(0));
    }

    return "";
}

/// Checks whether the indices in the GEP correspond to the indices in
/// the list. When one list is longer than the other, the function behaves
/// like one is cut to the size of the other and compares the rest.
/// \param[in] gep: The GEP operator. Both the instruction and the constant
/// expression are supported.
/// \param[in] indices: A list of integers representing indices to compare the
/// GEP operator with.
/// \return True or false based on whether the indices correspond.
bool checkGEPIndicesCorrespond(const GEPOperator *GEP,
                               std::vector<int> indices) {
    for (unsigned i = 1; i < GEP->getNumOperands(); i++) {
        if ((i - 1) >= indices.size()) {
            auto Op = GEP->getOperand(i);
            if (isa<ConstantInt>(Op)
                && (dyn_cast<ConstantInt>(Op)->getZExtValue()
                    != (unsigned)indices[i - 1])) {
                return false;
            }
        }
    }

    return true;
}

/// Find names of all functions using the given parameter (global variable).
std::set<StringRef> getFunctionsUsingParam(std::string Param,
                                           std::vector<int> indices,
                                           const Module *Mod) {
    auto Glob = Mod->getNamedGlobal(Param);
    if (!Glob)
        return std::set<StringRef>();

    std::set<StringRef> Result;
    for (const Use &U : Glob->uses()) {
        if (auto Inst = dyn_cast<Instruction>(U.getUser())) {
            // User is an instruction.
            auto Fun = Inst->getFunction();

            if (isa<GetElementPtrInst>(Inst) && !indices.empty()) {
                if (!checkGEPIndicesCorrespond(dyn_cast<GEPOperator>(Inst),
                                               indices))
                    continue;
            }

            if (!Fun->getName().contains(".old"))
                Result.insert(Fun->getName());
        } else if (auto CExpr = dyn_cast<ConstantExpr>(U.getUser())) {
            // User is a constant expression (typically GEP).
            for (const Use &UU : CExpr->uses()) {
                if (auto Inst = dyn_cast<Instruction>(UU.getUser())) {
                    auto Fun = Inst->getFunction();

                    if (isa<GEPOperator>(CExpr) && !indices.empty()) {
                        if (!checkGEPIndicesCorrespond(
                                    dyn_cast<GEPOperator>(CExpr), indices))
                            continue;
                    }

                    if (!Fun->getName().contains(".old"))
                        Result.insert(Fun->getName());
                }
            }
        }
    }

    return Result;
}

/// Find global variable in the module that corresponds to the given param.
/// In case the param is defined by module_param_named, this can be different
/// from the param name.
/// Information about the variable is stored inside the last element of the
/// structure assigned to the '__param_#name' variable (#name is the name of
/// param).
/// \param[in] Param Parameter name
/// \param[in] Mod Module in which the parameter resides
/// \return Name of the global variable corresponding to the parameter
StringRef findParamVar(std::string Param, const Module *Mod) {
    auto GlobVar = Mod->getNamedGlobal("__param_" + Param);
    if (!GlobVar)
        return "";

    // Get value of __param_#name variable.
    auto GlobValue = dyn_cast<ConstantStruct>(GlobVar->getInitializer());
    if (!GlobValue)
        return "";

    // Get the last element.
    auto VarUnion = dyn_cast<ConstantStruct>(
            GlobValue->getOperand(GlobValue->getNumOperands() - 1));
    if (!VarUnion)
        return "";
    if (VarUnion->getNumOperands() == 1) {
        auto Var = VarUnion->getOperand(0);
        return extractParamName(Var);
    }

    return "";
}
