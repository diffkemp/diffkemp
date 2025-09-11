//===--------- FunctionAbstractionsGeneratorTest.cpp - Unit tests ----------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the FunctionAbstractionsGenerator pass.
///
//===----------------------------------------------------------------------===//

#include <Utils.h>
#include <gtest/gtest.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/PassManagerImpl.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/CalledFunctionsAnalysis.h>
#include <passes/FunctionAbstractionsGenerator.h>

static void runFunctionAbstractionsGenerator(Module *Mod, Function *Fun) {
    AnalysisManager<Module, Function *> mam;
    mam.registerPass([] { return CalledFunctionsAnalysis(); });
    mam.registerPass([] { return PassInstrumentationAnalysis(); });
    mam.registerPass([] { return FunctionAbstractionsGenerator(); });
    mam.getResult<FunctionAbstractionsGenerator>(*Mod, Fun);
}

/// Creates a module and a function with four inline assembly calls.
/// Two inline assembly values are generated, each is called twice.
/// FunctionAbstractionsGenerator is then used to convert them into abstractions
/// and the result is checked.
TEST(FunctionAbstractionsGeneratorTest, InlineAsm) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create two different inline assembly values.
    InlineAsm *Asm1 =
            InlineAsm::get(FunctionType::get(Type::getVoidTy(Ctx), {}),
                           "inst1",
                           "constraint1",
                           false);
    InlineAsm *Asm2 =
            InlineAsm::get(FunctionType::get(Type::getVoidTy(Ctx), {}),
                           "inst2",
                           "constraint2",
                           true);

    // Create a function that calls both of the inline assembly values.
    Function *Fun = Function::Create(
            FunctionType::get(Type::getVoidTy(Mod->getContext()), {}),
            GlobalValue::ExternalLinkage,
            "test",
            Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Fun);

    // Call each inline asm twice to check if the inline asm to abstraction
    // matching works properly.
    CallInst::Create(Asm1, {ConstantInt::get(Type::getInt8Ty(Ctx), 0)}, "", BB);
    CallInst::Create(Asm2, {ConstantInt::get(Type::getInt8Ty(Ctx), 0)}, "", BB);
    CallInst::Create(Asm2, {ConstantInt::get(Type::getInt8Ty(Ctx), 1)}, "", BB);
    CallInst::Create(Asm1, {ConstantInt::get(Type::getInt8Ty(Ctx), 1)}, "", BB);
    ReturnInst::Create(Ctx, nullptr, BB);

    // Run the pass and check the result.
    runFunctionAbstractionsGenerator(Mod, Fun);

    std::vector<Instruction *> FunBody;
    for (Instruction &Inst : *BB)
        FunBody.push_back(&Inst);
    std::vector<Function *> Abstractions;
    ASSERT_EQ(FunBody.size(), 5);

    // Check if the calls to the abstractions are correct.
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(isa<CallInst>(FunBody[i]));
        auto Call = dyn_cast<CallInst>(FunBody[i]);
        ASSERT_TRUE(isa<Function>(getCallee(Call)));
        ASSERT_EQ(Call->arg_size(), 1);
        ASSERT_TRUE(isa<ConstantInt>(Call->getArgOperand(0)));
        ASSERT_EQ(dyn_cast<ConstantInt>(Call->getArgOperand(0))->getZExtValue(),
                  (i < 2) ? 0 : 1);
        auto CalledFun = Call->getCalledFunction();
        ASSERT_TRUE(isSimpllAbstractionDeclaration(CalledFun));
        Abstractions.push_back(CalledFun);
    }

    // Check whether the inline assembly metadata was assigned successfully.
    for (int i : {0, 3}) {
        ASSERT_EQ(getInlineAsmString(Abstractions[i]).str(), "inst1");
        ASSERT_EQ(getInlineAsmConstraintString(Abstractions[i]).str(),
                  "constraint1");
    }

    for (int i : {1, 2}) {
        ASSERT_EQ(getInlineAsmString(Abstractions[i]).str(), "inst2");
        ASSERT_EQ(getInlineAsmConstraintString(Abstractions[i]).str(),
                  "constraint2");
    }

    // Check whether the abstraction functions were generated correctly.
    ASSERT_EQ(Abstractions[0], Abstractions[3]);
    ASSERT_EQ(Abstractions[1], Abstractions[2]);
    ASSERT_NE(Abstractions[0], Abstractions[1]);
}

/// Creates a module and a function with four indirect calls through a function
/// pointer inside a global variable.
/// Two function types are used for the pointer, with two calls for each one of
/// them.
/// FunctionAbstractionsGenerator is then used to convert the indirect call into
/// a call to an abstraction and the result is checked.
TEST(FunctionAbstractionsGeneratorTest, IndirectCall) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create the global variables for use in the indirect call.
    FunctionType *FunTy1 = FunctionType::get(
            Type::getVoidTy(Ctx), {Type::getInt8Ty(Ctx)}, false);
    FunctionType *FunTy2 = FunctionType::get(
            Type::getVoidTy(Ctx), {Type::getInt16Ty(Ctx)}, false);
    GlobalVariable *FunPtr1 =
            new GlobalVariable(*Mod,
                               FunTy1,
                               false,
                               GlobalVariable::ExternalLinkage,
                               nullptr,
                               "funptr1");
    GlobalVariable *FunPtr2 =
            new GlobalVariable(*Mod,
                               FunTy1,
                               false,
                               GlobalVariable::ExternalLinkage,
                               nullptr,
                               "funptr2");
    GlobalVariable *FunPtr3 =
            new GlobalVariable(*Mod,
                               FunTy2,
                               false,
                               GlobalVariable::ExternalLinkage,
                               nullptr,
                               "funptr3");
    GlobalVariable *FunPtr4 =
            new GlobalVariable(*Mod,
                               FunTy2,
                               false,
                               GlobalVariable::ExternalLinkage,
                               nullptr,
                               "funptr4");

    // Create a function that calls the pointer.
    Function *Fun = Function::Create(
            FunctionType::get(Type::getVoidTy(Mod->getContext()), {}),
            GlobalValue::ExternalLinkage,
            "test",
            Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Fun);
    CallInst::Create(FunTy1,
                     FunPtr1,
                     {ConstantInt::get(Type::getInt8Ty(Ctx), 0)},
                     "",
                     BB);
    CallInst::Create(FunTy2,
                     FunPtr3,
                     {ConstantInt::get(Type::getInt16Ty(Ctx), 0)},
                     "",
                     BB);
    CallInst::Create(FunTy1,
                     FunPtr2,
                     {ConstantInt::get(Type::getInt8Ty(Ctx), 1)},
                     "",
                     BB);
    CallInst::Create(FunTy2,
                     FunPtr4,
                     {ConstantInt::get(Type::getInt16Ty(Ctx), 1)},
                     "",
                     BB);
    ReturnInst::Create(Ctx, BB);

    // Run the pass and check the result.
    runFunctionAbstractionsGenerator(Mod, Fun);

    std::vector<Instruction *> FunBody;
    for (Instruction &Inst : *BB)
        FunBody.push_back(&Inst);
    ASSERT_EQ(FunBody.size(), 5);

    std::vector<Value *> IndirectCallees;
    std::vector<Function *> Abstractions;
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(isa<CallInst>(FunBody[i]));
        auto Call = dyn_cast<CallInst>(FunBody[i]);
        ASSERT_TRUE(isa<Function>(getCallee(Call)));
        auto CalledFun = Call->getCalledFunction();
        ASSERT_TRUE(isSimpllAbstractionDeclaration(CalledFun));
        Abstractions.push_back(CalledFun);
        ASSERT_EQ(Call->arg_size(), 2);
        ASSERT_TRUE(isa<ConstantInt>(Call->getArgOperand(0)));
        ASSERT_EQ(dyn_cast<ConstantInt>(Call->getArgOperand(0))->getZExtValue(),
                  (i < 2) ? 0 : 1);
        IndirectCallees.push_back(Call->getArgOperand(1));
    }

    // Check whether the abstraction calls call the correct pointers.
    std::vector<Value *> ExpectedIndirectCallees{
            FunPtr1, FunPtr3, FunPtr2, FunPtr4};
    ASSERT_EQ(IndirectCallees, ExpectedIndirectCallees);

    // Check whether the abstraction functions were generated correctly.
    ASSERT_EQ(Abstractions[0], Abstractions[2]);
    ASSERT_EQ(Abstractions[1], Abstractions[3]);
    ASSERT_NE(Abstractions[0], Abstractions[3]);
}
