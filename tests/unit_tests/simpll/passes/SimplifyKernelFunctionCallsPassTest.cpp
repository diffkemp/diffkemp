//===------- SimplifyKernelFunctionCallsPassTest.cpp - Unit tests ----------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the SimplifyKernelFunctionCallsPass pass.
///
//===----------------------------------------------------------------------===//

#include <Utils.h>
#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/SimplifyKernelFunctionCallsPass.h>

/// Helper function that executes the pass on Fun.
static void simplifyKernelFunctions(Function *Fun) {
    FunctionPassManager fpm;
    FunctionAnalysisManager fam;
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(SimplifyKernelFunctionCallsPass{});
    fpm.run(*Fun, fam);
}

/// Tests replacement of simplifiable inline assembly call arguments by
/// SimplifyKernelFunctionCallsPass.
TEST(SimplifyKernelFunctionCallsPassTest, InlineAsm) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create a function containing an inline asm call with the string
    Function *Fun =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "test",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Fun);
    InlineAsm *Asm1 = InlineAsm::get(
            FunctionType::get(Type::getVoidTy(Ctx),
                              {PointerType::get(Type::getInt8Ty(Ctx), 0)},
                              true),
            "call __bug_table, $0, $1",
            "",
            true);
    InlineAsm *Asm2 = InlineAsm::get(
            FunctionType::get(Type::getVoidTy(Ctx),
                              {PointerType::get(Type::getInt8Ty(Ctx), 0)},
                              true),
            "call mars_landing, $0, $1",
            "",
            true);
    auto AuxPtr = CastInst::Create(Instruction::IntToPtr,
                                   ConstantInt::get(Type::getInt64Ty(Ctx), 1),
                                   PointerType::get(Type::getInt8Ty(Ctx), 0),
                                   "",
                                   BB);
    CallInst::Create(
            Asm1, {AuxPtr, ConstantInt::get(Type::getInt64Ty(Ctx), 1)}, "", BB);
    CallInst::Create(
            Asm2, {AuxPtr, ConstantInt::get(Type::getInt64Ty(Ctx), 2)}, "", BB);
    ReturnInst::Create(Ctx, BB);

    // Run the pass and check the results.
    simplifyKernelFunctions(Fun);

    auto Iter = BB->begin();
    ASSERT_NE(Iter, BB->end());
    ASSERT_EQ(&*Iter, AuxPtr);
    ++Iter;

    // The arguments of the first call should be replaced with null and zero.
    ASSERT_NE(Iter, BB->end());
    auto Call1 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(Call1);
    ASSERT_EQ(getCallee(Call1), Asm1);
    ASSERT_EQ(Call1->getNumArgOperands(), 2);
    ASSERT_EQ(Call1->getOperand(0)->getType(),
              PointerType::get(Type::getInt8Ty(Ctx), 0));
    ASSERT_TRUE(isa<ConstantPointerNull>(Call1->getOperand(0)));
    ASSERT_EQ(Call1->getOperand(1)->getType(), Type::getInt64Ty(Ctx));
    ASSERT_TRUE(isa<ConstantInt>(Call1->getOperand(1)));
    ASSERT_EQ(dyn_cast<ConstantInt>(Call1->getOperand(1))->getZExtValue(), 0);
    ++Iter;

    // The second call should be unmodified.
    ASSERT_NE(Iter, BB->end());
    auto Call2 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(Call2);
    ASSERT_EQ(getCallee(Call2), Asm2);
    ASSERT_EQ(Call2->getNumArgOperands(), 2);
    ASSERT_EQ(Call2->getOperand(0), AuxPtr);
    ASSERT_EQ(Call2->getOperand(1)->getType(), Type::getInt64Ty(Ctx));
    ASSERT_TRUE(isa<ConstantInt>(Call2->getOperand(1)));
    ASSERT_EQ(dyn_cast<ConstantInt>(Call2->getOperand(1))->getZExtValue(), 2);
    ++Iter;

    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
    ++Iter;
    ASSERT_EQ(Iter, BB->end());
}

/// Tests replacement of simplifiable print function call arguments by
/// SimplifyKernelFunctionCallsPass.
TEST(SimplifyKernelFunctionCallsPassTest, PrintFun) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create two print functions (two kinds of them are handled by the pass).
    auto FunPrintk = Function::Create(
            FunctionType::get(Type::getInt32Ty(Ctx),
                              {PointerType::get(Type::getInt8Ty(Ctx), 0)},
                              true),
            GlobalValue::ExternalLinkage,
            "printk",
            Mod);
    auto FunDevWarn = Function::Create(
            FunctionType::get(Type::getInt32Ty(Ctx),
                              {PointerType::get(Type::getInt8Ty(Ctx), 0),
                               PointerType::get(Type::getInt8Ty(Ctx), 0)},
                              true),
            GlobalValue::ExternalLinkage,
            "dev_warn",
            Mod);

    // Create the main function with a call to each print function.
    Function *Main =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "main",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Main);
    auto AuxPtr = CastInst::Create(Instruction::IntToPtr,
                                   ConstantInt::get(Type::getInt64Ty(Ctx), 1),
                                   PointerType::get(Type::getInt8Ty(Ctx), 0),
                                   "",
                                   BB);
    IRBuilder<> Builder(BB);
    auto FmtStr = Builder.CreateGlobalString("%d");
    CallInst::Create(FunPrintk,
                     {FmtStr, ConstantInt::get(Type::getInt32Ty(Ctx), 1)},
                     "",
                     BB);
    CallInst::Create(
            FunDevWarn,
            {AuxPtr, FmtStr, ConstantInt::get(Type::getInt32Ty(Ctx), 2)},
            "",
            BB);
    ReturnInst::Create(Ctx, BB);

    // Run the pass and check the results.
    simplifyKernelFunctions(Main);

    auto Iter = BB->begin();
    ASSERT_NE(Iter, BB->end());
    ASSERT_EQ(&*Iter, AuxPtr);
    ++Iter;

    // Both calls should have two nulls as their arguments.
    for (int i = 0; i < 2; i++) {
        ASSERT_NE(Iter, BB->end());
        auto Call = dyn_cast<CallInst>(&*Iter);
        ASSERT_TRUE(Call);
        ASSERT_EQ(Call->getCalledFunction(), (i == 0) ? FunPrintk : FunDevWarn);
        ASSERT_EQ(Call->getNumArgOperands(), 2);
        ASSERT_TRUE(isa<ConstantPointerNull>(Call->getOperand(0)));
        ASSERT_TRUE(isa<ConstantPointerNull>(Call->getOperand(1)));
        ++Iter;
    }

    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
    ++Iter;
    ASSERT_EQ(Iter, BB->end());
}

/// Tests replacement of simplifiable debug function call arguments by
/// SimplifyKernelFunctionCallsPass.
/// Note: in real code these calls contain the line number from the C source
/// code.
TEST(SimplifyKernelFunctionCallsPassTest, DebugFun) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create a debug function.
    auto FunMightSleep = Function::Create(
            FunctionType::get(Type::getVoidTy(Ctx),
                              {PointerType::get(Type::getInt8Ty(Ctx), 0),
                               Type::getInt32Ty(Ctx),
                               Type::getInt32Ty(Ctx)},
                              true),
            GlobalValue::ExternalLinkage,
            "__might_sleep",
            Mod);

    // Create the main function with a call to it.
    Function *Main =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "main",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Main);
    IRBuilder<> Builder(BB);
    CallInst::Create(FunMightSleep,
                     {Builder.CreateGlobalString("test"),
                      ConstantInt::get(Type::getInt32Ty(Ctx), 1),
                      ConstantInt::get(Type::getInt32Ty(Ctx), 2)},
                     "",
                     BB);
    ReturnInst::Create(Ctx, BB);

    // Run the pass and check the results.
    simplifyKernelFunctions(Main);

    auto Iter = BB->begin();

    ASSERT_NE(Iter, BB->end());
    auto Call = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(Call);
    ASSERT_EQ(Call->getCalledFunction(), FunMightSleep);
    ASSERT_EQ(Call->getNumArgOperands(), 3);
    ASSERT_TRUE(isa<ConstantPointerNull>(Call->getOperand(0)));
    ASSERT_EQ(Call->getOperand(1)->getType(), Type::getInt32Ty(Ctx));
    ASSERT_TRUE(isa<ConstantInt>(Call->getOperand(1)));
    ASSERT_EQ(dyn_cast<ConstantInt>(Call->getOperand(1))->getZExtValue(), 0);
    ASSERT_EQ(Call->getOperand(2)->getType(), Type::getInt32Ty(Ctx));
    ASSERT_TRUE(isa<ConstantInt>(Call->getOperand(2)));
    ASSERT_EQ(dyn_cast<ConstantInt>(Call->getOperand(2))->getZExtValue(), 2);
    ++Iter;

    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
    ++Iter;
    ASSERT_EQ(Iter, BB->end());
}
