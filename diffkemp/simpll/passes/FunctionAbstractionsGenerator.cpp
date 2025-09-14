//===- FunctionAbstractionsGenerator.cpp - Abstracting non-function calls -===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains functions generating and unifying abstractions for
/// indirect function calls and for inline assemblies.
///
//===----------------------------------------------------------------------===//

#include "FunctionAbstractionsGenerator.h"
#include "CalledFunctionsAnalysis.h"
#include "Logger.h"
#include "Utils.h"
#include <Config.h>

#include <llvm/ADT/Hashing.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

AnalysisKey FunctionAbstractionsGenerator::Key;

/// Creates a new function for each type of function that is called indirectly
/// and for each pair of assembly code and constraint.
FunctionAbstractionsGenerator::Result FunctionAbstractionsGenerator::run(
        Module &Mod, AnalysisManager<Module, Function *> &mam, Function *Main) {
    LOG_NO_INDENT("Generating function abstractions in " << Mod.getName()
                                                         << "...\n");
    LOG_INDENT();
    FunMap funAbstractions;
    int i = 0;
    std::vector<Instruction *> toErase;

    auto &CalledFuns = mam.getResult<CalledFunctionsAnalysis>(Mod, Main);

    for (auto &Fun : Mod) {
        if (CalledFuns.find(&Fun) == CalledFuns.end())
            continue;
        for (auto &BB : Fun) {
            for (auto &Instr : BB) {
                if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                    auto funCalled = getCalledFunction(CallInstr);
                    if (funCalled)
                        continue;

                    Value *Callee = getCallee(CallInstr);
                    auto CalledType = Callee->getType();
                    if (!CalledType->isPointerTy())
                        continue;

                    auto FunType = CallInstr->getFunctionType();

                    // Retrieve function from the hash map if it has already
                    // been created or create a new one.
                    std::string hash = funHash(CallInstr);
                    auto funAbstr = funAbstractions.find(hash);
                    Function *newFun;

                    if (funAbstr == funAbstractions.end()) {
                        std::vector<Type *> newParamTypes;
                        newParamTypes = FunType->params();
                        if (!CallInstr->isInlineAsm())
                            newParamTypes.push_back(CalledType);
                        auto newFunType = FunctionType::get(
                                FunType->getReturnType(), newParamTypes, false);

                        const std::string funName =
                                abstractionPrefix(Callee) + std::to_string(i++);

                        newFun = Function::Create(newFunType,
                                                  Function::ExternalLinkage,
                                                  funName,
                                                  &Mod);
                        funAbstractions.emplace(hash, newFun);
                        if (auto assembly = dyn_cast<InlineAsm>(Callee)) {
                            // Add metadata that will be used for comparison
                            // to the abstraction.
                            MDString *asmMD =
                                    MDString::get(newFun->getContext(),
                                                  assembly->getAsmString());
                            MDString *constraintMD = MDString::get(
                                    newFun->getContext(),
                                    assembly->getConstraintString());
                            MDNode *metadata =
                                    MDTuple::get(newFun->getContext(),
                                                 {asmMD, constraintMD});
                            newFun->setMetadata("inlineasm", metadata);
                        }
                    } else {
                        newFun = funAbstr->second;
                    }

                    // Transform the call to a call to the abstraction
                    std::vector<Value *> args;
                    for (auto &a : CallInstr->args()) {
                        if (auto argVal = dyn_cast<Value>(&a))
                            args.push_back(argVal);
                    }
                    if (!CallInstr->isInlineAsm())
                        args.push_back(Callee);
                    auto newCall =
                            CallInst::Create(newFun, args, "", CallInstr);
                    newCall->setDebugLoc(CallInstr->getDebugLoc());
                    LOG_VERBOSE_EXTRA_NO_INDENT("Replacing :"
                                                << *CallInstr << "\n     with :"
                                                << *newCall << "\n");
                    CallInstr->replaceAllUsesWith(newCall);
                    toErase.push_back(&Instr);
                }
            }
            for (auto &I : toErase)
                I->eraseFromParent();
            toErase.clear();
        }
    }
    LOG_UNINDENT();
    return FunctionAbstractionsGenerator::Result{funAbstractions};
}

/// A hash that uniquely identifies an indirect function or an inline asm.
/// It contains the string representing the function type, and for inline asm
/// also the assembly parameters and code.
std::string FunctionAbstractionsGenerator::funHash(CallInst *Call) {
    std::string result = typeName(Call->getFunctionType());
    if (auto inlineAsm = dyn_cast<InlineAsm>(getCallee(Call))) {
        result += "$" + inlineAsm->getAsmString() + "$"
                  + inlineAsm->getConstraintString();
    }
    return result;
}

std::string FunctionAbstractionsGenerator::abstractionPrefix(Value *Fun) {
    if (isa<InlineAsm>(Fun))
        return SimpllInlineAsmPrefix;
    else
        return SimpllIndirectFunctionPrefix;
}

/// Swaps names of two functions in a module.
/// \param Map Function hash map of the appropriate module.
/// \param SrcHash Hash of one of the functions.
/// \param DestName Name of the other of the functions.
/// \return True if the swap was successful.
bool trySwap(FunctionAbstractionsGenerator::FunMap &Map,
             const std::string &SrcHash,
             const std::string &DestName) {
    for (auto &Fun : Map) {
        if (Fun.second->getName() == DestName) {
            const std::string srcName =
                    Map.find(SrcHash)->second->getName().str();
            Map.find(SrcHash)->second->setName("$tmpName");
            Fun.second->setName(srcName);
            Map.find(SrcHash)->second->setName(DestName);
            return true;
        }
    }
    return false;
}

/// Returns true if the function is an abstraction generated by
/// FunctionAbstractionsGenerator.
bool isSimpllAbstractionDeclaration(const Function *Fun) {
    return hasPrefix(Fun->getName(), SimpllIndirectFunctionPrefix)
           || hasPrefix(Fun->getName(), SimpllInlineAsmPrefix);
}

/// Returns true if the function is a SimpLL abstraction.
bool isSimpllAbstraction(const Function *Fun) {
    return isSimpllAbstractionDeclaration(Fun);
}

/// Extracts inline assembly code string from abstraction.
StringRef getInlineAsmString(const Function *Abstr) {
    MDTuple *metadata = cast<MDTuple>(Abstr->getMetadata("inlineasm"));
    return cast<MDString>(*(metadata->getOperand(0))).getString();
}

/// Extracts inline assembly code constraint string from abstraction.
StringRef getInlineAsmConstraintString(const Function *Abstr) {
    MDTuple *metadata = cast<MDTuple>(Abstr->getMetadata("inlineasm"));
    return cast<MDString>(*(metadata->getOperand(1))).getString();
}
