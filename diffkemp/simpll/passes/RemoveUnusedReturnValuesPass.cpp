//===--- RemoveUnusedReturnValuesPass.h - Transforming functions to void --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the RemoveUnusedReturnValues pass.
///
//===----------------------------------------------------------------------===//

#include "RemoveUnusedReturnValuesPass.h"
#include "CalledFunctionsAnalysis.h"
#include "FunctionAbstractionsGenerator.h"
#include "Utils.h"
#include <Config.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Transforms/Utils/Cloning.h>

PreservedAnalyses RemoveUnusedReturnValuesPass::run(
        Module &Mod,
        AnalysisManager<Module, Function *> &mam,
        Function *Main,
        Module *ModOther) {

    // These attributes are invalid for void functions
    Attribute::AttrKind badAttributes[] = {
            Attribute::AttrKind::ByVal,
            Attribute::AttrKind::InAlloca,
            Attribute::AttrKind::Nest,
            Attribute::AttrKind::NoAlias,
            Attribute::AttrKind::NoCapture,
            Attribute::AttrKind::NonNull,
            Attribute::AttrKind::ReadNone,
            Attribute::AttrKind::ReadOnly,
            Attribute::AttrKind::SExt,
            Attribute::AttrKind::StructRet,
            Attribute::AttrKind::ZExt,
            Attribute::AttrKind::Dereferenceable,
            Attribute::AttrKind::DereferenceableOrNull};

    auto &CalledFuns = mam.getResult<CalledFunctionsAnalysis>(Mod, Main);

    // Initial list of functions to iterate over.
    std::vector<Function *> functionsToIterateOver;
    for (Function &Fun : Mod)
        functionsToIterateOver.push_back(&Fun);

    for (Function *Fun : functionsToIterateOver) {
        if (Fun->getIntrinsicID() != llvm::Intrinsic::not_intrinsic)
            continue;

        if (Fun->getReturnType()->isVoidTy())
            continue;

        if (!isSimpllAbstractionDeclaration(Fun)) {
            if (!ModOther->getFunction(Fun->getName()))
                continue;

            if (!ModOther->getFunction(Fun->getName())
                         ->getReturnType()
                         ->isVoidTy())
                continue;

            if (CalledFuns.find(Fun) == CalledFuns.end())
                continue;
        }

        std::vector<Instruction *> toReplace;

        for (Use &U : Fun->uses()) {
            // Figure out whether the return value is used after each call.
            if (U.getUser()->use_empty() &&
                ((isa<CallInst>(U.getUser()) &&
                  (dyn_cast<CallInst>(U.getUser())->getCalledFunction()) ==
                          Fun) ||
                 (isa<InvokeInst>(U.getUser()) &&
                  (dyn_cast<InvokeInst>(U.getUser())->getCalledFunction()) ==
                          Fun))) {
                // The instruction can be replaced.
                toReplace.push_back(dyn_cast<Instruction>(U.getUser()));
            }
        }

        if (toReplace.empty())
            // Nothing to replace.
            continue;

        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent(' ')
                               << "Creating void-returning variant of "
                               << Fun->getName() << "\n");

        // Create a clone of the function.
        // Note: this is needed because the arguments of the original function
        // are going to be replaced with the arguments of the replacement
        // function in the whole module including the original function, which
        // ends up unusable, and therefore is deleted and replaced with the
        // clone. (Another solution would be to replace the uses manually, but
        // this is an easier solution.)
        ValueToValueMapTy Map;
        std::string OriginalName = Fun->getName();
        if (hasSuffix(OriginalName))
            OriginalName = dropSuffix(OriginalName);
        Fun->setName("");
        Function *Fun_Clone;
        if (!Fun->isDeclaration())
            Fun_Clone = CloneFunction(Fun, Map);
        else {
            // CloneFunction support only functions with a full body (possibly
            // because of a bug). Handle declarations separately.
            Fun_Clone = Function::Create(Fun->getFunctionType(),
                                         Fun->getLinkage(),
                                         OriginalName,
                                         Fun->getParent());
            Fun_Clone->copyAttributesFrom(Fun);
            Fun_Clone->setSubprogram(Fun->getSubprogram());
            for (Function::arg_iterator AI = Fun->arg_begin(),
                                        AE = Fun->arg_end(),
                                        NAI = Fun_Clone->arg_begin();
                 AI != AE;
                 ++AI, ++NAI) {
                NAI->takeName(AI);
            }
        }
        Fun_Clone->setName(OriginalName);

        // Create the header of the new function.
        std::vector<Type *> FAT_New(Fun->getFunctionType()->param_begin(),
                                    Fun->getFunctionType()->param_end());
        FunctionType *FT_New = FunctionType::get(
                Type::getVoidTy(Fun->getContext()), FAT_New, Fun->isVarArg());
        Function *Fun_New = Function::Create(
                FT_New, Fun->getLinkage(), Fun->getName(), Fun->getParent());

        // Copy the attributes from the old function and delete the ones
        // related to the return value
        Fun_New->copyAttributesFrom(Fun);
        for (Attribute::AttrKind AK : badAttributes) {
            Fun_New->removeAttribute(AttributeList::ReturnIndex, AK);
            Fun_New->removeAttribute(AttributeList::FunctionIndex, AK);
        }
        Fun_New->setAttributes(cleanAttributeList(Fun_New->getAttributes()));

        // Set the right function name and subprogram
        Fun_New->setName(OriginalName);
        Fun_New->setSubprogram(Fun->getSubprogram());

        // Set the names of all arguments of the new function
        for (Function::arg_iterator AI = Fun->arg_begin(),
                                    AE = Fun->arg_end(),
                                    NAI = Fun_New->arg_begin();
             AI != AE;
             ++AI, ++NAI) {
            NAI->takeName(AI);
        }

        // Copy the function body.
        Fun_New->getBasicBlockList().splice(Fun_New->begin(),
                                            Fun->getBasicBlockList());

        // Replace return instructions on ends of basic blocks with ret void
        for (BasicBlock &B : *Fun_New)
            if (dyn_cast<ReturnInst>(B.getTerminator())) {
                B.getInstList().pop_back();
                ReturnInst *Term_New = ReturnInst::Create(B.getContext());
                B.getInstList().push_back(Term_New);

                // Simplify the function to remove any code that became dead
                simplifyFunction(Fun_New);
            }

        // Replace all uses of the old arguments
        for (Function::arg_iterator I = Fun->arg_begin(),
                                    E = Fun->arg_end(),
                                    NI = Fun_New->arg_begin();
             I != E;
             ++I, ++NI) {
            I->replaceAllUsesWith(NI);
        }

        // For call or invoke instructions where the return value is not used
        // a new instruction has to be created and the old one replaced.
        for (Instruction *I : toReplace) {
            if (CallInst *CI = dyn_cast<CallInst>(I)) {
                // First copy all arguments to an array
                // and create the new instruction
                std::vector<Value *> Args;

                for (Value *A : CI->arg_operands()) {
                    Args.push_back(A);
                }

                ArrayRef<Value *> Args_AR(Args);

                // Insert the new instruction next to the old one
                CallInst *CI_New = CallInst::Create(Fun_New, Args_AR, "", CI);

                // Copy additional properties
                CI_New->setAttributes(CI->getAttributes());
                for (Attribute::AttrKind AK : badAttributes) {
                    // Remove incompatibile attributes
                    CI_New->removeAttribute(AttributeList::ReturnIndex, AK);
                    CI_New->removeAttribute(AttributeList::FunctionIndex, AK);
                }
                CI_New->setAttributes(
                        cleanAttributeList(CI_New->getAttributes()));
                CI_New->setDebugLoc(CI->getDebugLoc());
                CI_New->setCallingConv(CI->getCallingConv());
                if (CI->isTailCall())
                    CI_New->setTailCall();
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << "Replacing :" << *CI << " with "
                                       << *CI_New << "\n");
                // Erase the old instruction
                CI->eraseFromParent();
            } else if (InvokeInst *II = dyn_cast<InvokeInst>(I)) {
                // First copy all arguments to an array and create
                // the new instruction
                std::vector<Value *> Args;

                for (Value *A : II->arg_operands()) {
                    Args.push_back(A);
                }

                ArrayRef<Value *> Args_AR(Args);

                // Insert the new instruction next to the old one
                InvokeInst *II_New = InvokeInst::Create(Fun_New,
                                                        II->getNormalDest(),
                                                        II->getUnwindDest(),
                                                        Args_AR,
                                                        "",
                                                        II);

                // Copy additional properties
                II_New->setAttributes(II->getAttributes());
                for (Attribute::AttrKind AK : badAttributes) {
                    // Remove incompatibile attributes
                    II_New->removeAttribute(AttributeList::ReturnIndex, AK);
                    II_New->removeAttribute(AttributeList::FunctionIndex, AK);
                }
                II_New->setAttributes(
                        cleanAttributeList(II_New->getAttributes()));
                II_New->setDebugLoc(II->getDebugLoc());
                II_New->setCallingConv(II->getCallingConv());
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << "Replacing :" << *II << " with "
                                       << *II_New << "\n");
                // Erase the old instruction
                II->eraseFromParent();
            }
        }

        // Replace all other uses of the function with its clone.
        Fun->replaceAllUsesWith(Fun_Clone);

        // Delete function.
        Fun->eraseFromParent();
    }

    return PreservedAnalyses();
}
