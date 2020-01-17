//= FieldAccessFunctionGenerator.cpp - Move field access blocks to function ==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the FieldAccessFunctionGenerator pass.
///
//===----------------------------------------------------------------------===//

#include "FieldAccessFunctionGenerator.h"

#include "CalledFunctionsAnalysis.h"
#include "Utils.h"
#include <llvm/IR/Instructions.h>

PreservedAnalyses FieldAccessFunctionGenerator::run(
        Module &Mod,
        AnalysisManager<Module, Function *> &mam,
        Function *Main,
        Module *ModOther) {
    // This pass (similarly to RemoveUnusedReturnValuesPass, with which it
    // shares the same template) is only performed on functions that are called
    // somewhere in the compared code. Also although this is a pass on
    // functions, it has to be implemented as a module pass, because it adds new
    // functions to the module.
    auto &CalledFuns = mam.getResult<CalledFunctionsAnalysis>(Mod, Main);

    for (Function &Fun : Mod) {
        if (!ModOther->getFunction(Fun.getName()))
            continue;

        if (CalledFuns.find(&Fun) == CalledFuns.end())
            continue;

        std::vector<Instruction *> InstStack;

        // This stores the debug location for the currently processed stack.
        // This enables the stopping of the building of the stack when an
        // instruction with a different DILocation is reached.
        DILocation *DIL;

        // This is a FSM-like algorithm that gathers together groups of
        // instructions beginning with a GEP and containing GEPs and casts.
        // (They also have to have the same debug location, i.e. correspond to
        // a single field access in the C source code).
        // A vector/stack is generated from each of them and the processStack
        // function is called to further process the instructions.
        for (auto &BB : Fun) {
            for (auto &Inst : BB) {
                if (isa<GetElementPtrInst>(Inst) && InstStack.empty()) {
                    // Possible start of field access block.
                    DIL = Inst.getDebugLoc().get();
                    InstStack.push_back(&Inst);
                } else if (isa<GetElementPtrInst>(Inst)
                           || (isa<CastInst>(Inst) && !isa<PtrToIntInst>(Inst))
                                      && !InstStack.empty()) {
                    if (Inst.getDebugLoc().get() != DIL) {
                        // Stop building stack.
                        processStack(InstStack, Mod);
                        InstStack.clear();
                        if (isa<GetElementPtrInst>(Inst)) {
                            // If the instruction is a GEP, start building a
                            // new stack.
                            DIL = Inst.getDebugLoc().get();
                            InstStack.push_back(&Inst);
                        }
                    } else {
                        // Add instruction to stack.
                        InstStack.push_back(&Inst);
                    }
                } else {
                    // Wrong instruction type. Stop building stack.
                    processStack(InstStack, Mod);
                    InstStack.clear();
                }
            }
            // Avoid processing instruction groups that contain instructions in
            // two or more basic blocks.
            processStack(InstStack, Mod);
            InstStack.clear();
        }
    }

    return PreservedAnalyses();
}

/// First checks whether the instructions in the stack can be moved away from
/// the original function without causing an error, then creates the abstraction
/// function, moves the instructions into it, modify the input and output of the
/// block to match the argument and returns value of the function and inserts
/// the call to the newly create abstraction instead of the instruction block to
/// the original function.
void FieldAccessFunctionGenerator::processStack(
        const std::vector<Instruction *> &Stack, Module &Mod) {
    if (Stack.empty())
        // Do not attempt to process empty stacks, that would cause an error.
        return;

    // Check if all the instruction (besides the last one, of course) are used
    // only by other instructions in the stack.
    // If not, do not generate an abstraction, since this would break the code.
    for (Instruction *Inst : Stack) {
        if (Inst == Stack.back())
            continue;
        for (User *U : Inst->users()) {
            if (std::find(Stack.begin(), Stack.end(), U) == std::end(Stack))
                return;
        }
    }

    // Check if all operands in the code are values included in the stack.
    // If not, arrange them to be passed to the generated abstraction.
    // Note: the first argument of the abstraction is always the operand of the
    // GEP - this should not be changed, since it is expected to be this way
    // later in the analysis.
    std::vector<Value *> valuesToReplace;
    for (Instruction *Inst : Stack) {
        for (Value *Op : Inst->operands()) {
            if (isa<Constant>(Op))
                // Constants are fine, they can be moved without problem.
                continue;
            if (std::find(Stack.begin(), Stack.end(), Op) == std::end(Stack)
                && std::find(valuesToReplace.begin(), valuesToReplace.end(), Op)
                           == std::end(valuesToReplace)) {
                // The value was not found in the stack.
                // Add it to the replacement vector (unless it is already
                // there).
                valuesToReplace.push_back(Op);
            }
        }
    }

    // Create the function definition.
    // The generated abstraction receives the source variable as its argument
    // and returns the result of the field access (both are pointers, as with
    // simple GEPs).
    std::vector<Type *> argTypes;
    std::transform(valuesToReplace.begin(),
                   valuesToReplace.end(),
                   std::back_inserter(argTypes),
                   [](Value *V) { return V->getType(); });
    FunctionType *FT =
            FunctionType::get(Stack.back()->getType(), argTypes, false);
    Function *Abstraction =
            Function::Create(FT,
                             GlobalValue::LinkageTypes::ExternalLinkage,
                             SimpllFieldAccessFunName,
                             &Mod);
    Abstraction->setMetadata(SimpllFieldAccessMetadata,
                             Stack.front()->getDebugLoc().get());

    // Create a map that will be used for replacing operands referencing values
    // outside of the abstraction function with its arguments.
    std::map<Value *, Value *> valueReplacementMap;
    int i = 0;
    for (Value &Arg : Abstraction->args()) {
        valueReplacementMap.insert({valuesToReplace[i++], &Arg});
    }

    // Copy the instructions to the function body.
    BasicBlock *BB =
            BasicBlock::Create(Abstraction->getContext(), "", Abstraction);
    for (Instruction *Inst : Stack) {
        // Move into the new function.
        if (Inst == Stack.back()) {
            // The last instruction needs special processing, because it will
            // be replaced with the function call in the original function.
            Instruction *Clone = Inst->clone();
            Clone->setDebugLoc(DebugLoc(
                    Abstraction->getMetadata(SimpllFieldAccessMetadata)));
            auto CallInst = CallInst::Create(FT, Abstraction, valuesToReplace);
            CallInst->insertAfter(Inst);
            CallInst->setDebugLoc(DebugLoc(
                    Abstraction->getMetadata(SimpllFieldAccessMetadata)));
            Inst->replaceAllUsesWith(CallInst);
            Inst->removeFromParent();
            BB->getInstList().push_back(Clone);
        } else {
            Inst->removeFromParent();
            Inst->setDebugLoc(DebugLoc(
                    Abstraction->getMetadata(SimpllFieldAccessMetadata)));
            BB->getInstList().push_back(Inst);
        }
        // Replace the instruction's operands with function arguments if needed.
        Instruction *NewInst = &BB->getInstList().back();
        for (int i = 0; i < NewInst->getNumOperands(); i++) {
            if (valueReplacementMap.find(NewInst->getOperand(i))
                != valueReplacementMap.end()) {
                NewInst->setOperand(
                        i, valueReplacementMap[NewInst->getOperand(i)]);
            }
        }
    }

    // Create return instruction
    auto ReturnInst =
            ReturnInst::Create(Abstraction->getContext(), &BB->back(), BB);
}

/// Returns true if the function is an SimpLL field access abstraction.
bool isSimpllFieldAccessAbstraction(const Function *Fun) {
    return Fun->getName().startswith(SimpllFieldAccessFunName);
}
