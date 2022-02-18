//===---------- SeparateCallsToBitcastPassTest.cpp - Unit tests ------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the SeparateCallsToBitcastPass pass.
///
//===----------------------------------------------------------------------===//

#include <Utils.h>
#include <gtest/gtest.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/SeparateCallsToBitcastPass.h>

/// Creates two functions declarations for the testing of bitcast inlining.
/// The first one has a constant number of arguments and returns void,
/// the second one returns int and has a variable number of arguments.
/// A third function is then created to serve as the main function, calling
/// the first two in various ways with different casts.
/// Finally the pass is run on the main function and the pass results are
/// checked.
TEST(SeparateCallsToBitcastPassTest, Base) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create the functions declarations for testing.
    Function *Fun1 = Function::Create(
            FunctionType::get(Type::getVoidTy(Ctx),
                              {Type::getInt8Ty(Ctx), Type::getInt16Ty(Ctx)},
                              false),
            GlobalValue::ExternalLinkage,
            "fun1",
            Mod);
    Function *Fun2 = Function::Create(FunctionType::get(Type::getInt8Ty(Ctx),
                                                        {Type::getInt8Ty(Ctx)},
                                                        true),
                                      GlobalValue::ExternalLinkage,
                                      "fun2",
                                      Mod);

    // Create the main function and a few bitcast call of various types.
    Function *Main =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "main",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Main);

    // Cast the void return value to integer. This call should not be processed,
    // because by replacing the cast the return value would be lost.
    // Note: this would of course break the stack if executed, nevertheless it
    // has to be taken into account when doing static analysis.
    FunctionType *NewType =
            FunctionType::get(Type::getInt8Ty(Ctx),
                              {Type::getInt8Ty(Ctx), Type::getInt16Ty(Ctx)},
                              false);
    CallInst *Call1 = CallInst::Create(
#if LLVM_VERSION_MAJOR >= 8
            NewType,
#endif
            ConstantExpr::getCast(
                    Instruction::BitCast, Fun1, NewType->getPointerTo()),
            {ConstantInt::get(Type::getInt8Ty(Ctx), 0),
             ConstantInt::get(Type::getInt16Ty(Ctx), 1)},
            "",
            BB);

    // Cast one of the arguments to a different integer size.
    NewType = FunctionType::get(Type::getVoidTy(Ctx),
                                {Type::getInt16Ty(Ctx), Type::getInt16Ty(Ctx)},
                                false);
    CallInst *Call2 = CallInst::Create(
#if LLVM_VERSION_MAJOR >= 8
            NewType,
#endif
            ConstantExpr::getCast(
                    Instruction::BitCast, Fun1, NewType->getPointerTo()),
            {ConstantInt::get(Type::getInt16Ty(Ctx), 0),
             ConstantInt::get(Type::getInt16Ty(Ctx), 1)},
            "",
            BB);

    // Reduce the argument number by casting. This cannot be replaced by the
    // pass because the other argument is missing in the call.
    NewType = FunctionType::get(
            Type::getVoidTy(Ctx), {Type::getInt8Ty(Ctx)}, false);
    CallInst *Call3 = CallInst::Create(
#if LLVM_VERSION_MAJOR >= 8
            NewType,
#endif
            ConstantExpr::getCast(
                    Instruction::BitCast, Fun1, NewType->getPointerTo()),
            {ConstantInt::get(Type::getInt8Ty(Ctx), 0)},
            "",
            BB);

    // Cast the first (non-vararg) argument.
    NewType = FunctionType::get(
            Type::getInt8Ty(Ctx), {Type::getInt16Ty(Ctx)}, true);
    CallInst *Call4 = CallInst::Create(
#if LLVM_VERSION_MAJOR >= 8
            NewType,
#endif
            ConstantExpr::getCast(
                    Instruction::BitCast, Fun2, NewType->getPointerTo()),
            {ConstantInt::get(Type::getInt16Ty(Ctx), 0),
             ConstantInt::get(Type::getInt8Ty(Ctx), 0)},
            "",
            BB);

    ReturnInst::Create(Ctx, BB);

    // Run the pass and check the results.
    FunctionPassManager fpm;
    FunctionAnalysisManager fam;
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(SeparateCallsToBitcastPass{});
    fpm.run(*Main, fam);

    auto Iter = BB->begin();

    // The first call should be unmodified.
    ASSERT_NE(Iter, BB->end());
    auto TestCall1 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(TestCall1);
    ASSERT_EQ(Call1, TestCall1);
    ASSERT_TRUE(isa<BitCastOperator>(getCallee(TestCall1)));
    ASSERT_EQ(dyn_cast<BitCastOperator>(getCallee(TestCall1))
                      ->stripPointerCasts(),
              Fun1);
    ASSERT_EQ(TestCall1->getType(), Type::getInt8Ty(Ctx));
    ASSERT_EQ(TestCall1->getNumArgOperands(), 2);
    ASSERT_TRUE(isa<ConstantInt>(TestCall1->getArgOperand(0)));
    ASSERT_TRUE(isa<ConstantInt>(TestCall1->getArgOperand(1)));
    ASSERT_EQ(TestCall1->getArgOperand(0)->getType(), Type::getInt8Ty(Ctx));
    ASSERT_EQ(TestCall1->getArgOperand(1)->getType(), Type::getInt16Ty(Ctx));
    ASSERT_EQ(
            dyn_cast<ConstantInt>(TestCall1->getArgOperand(0))->getZExtValue(),
            0);
    ASSERT_EQ(
            dyn_cast<ConstantInt>(TestCall1->getArgOperand(1))->getZExtValue(),
            1);
    ++Iter;

    // The second call should be split into a cast and a direct call.
    ASSERT_NE(Iter, BB->end());
    auto TestCast1 = dyn_cast<CastInst>(&*Iter);
    ASSERT_TRUE(TestCast1);
    ASSERT_EQ(TestCast1->getOpcode(), Instruction::BitCast);
    ASSERT_EQ(TestCast1->getSrcTy(), Type::getInt16Ty(Ctx));
    ASSERT_EQ(TestCast1->getDestTy(), Type::getInt8Ty(Ctx));
    ASSERT_TRUE(isa<ConstantInt>(TestCast1->getOperand(0)));
    ASSERT_EQ(dyn_cast<ConstantInt>(TestCast1->getOperand(0))->getZExtValue(),
              0);
    ++Iter;
    ASSERT_NE(Iter, BB->end());
    auto TestCall2 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(TestCall2);
    ASSERT_EQ(TestCall2->getCalledFunction(), Fun1);
    ASSERT_EQ(TestCall2->getNumArgOperands(), 2);
    ASSERT_EQ(TestCall2->getArgOperand(0), TestCast1);
    ASSERT_TRUE(isa<ConstantInt>(TestCall2->getArgOperand(1)));
    ASSERT_EQ(TestCall2->getArgOperand(1)->getType(), Type::getInt16Ty(Ctx));
    ASSERT_EQ(
            dyn_cast<ConstantInt>(TestCall2->getArgOperand(1))->getZExtValue(),
            1);
    ++Iter;

    // The third call should be unmodified.
    ASSERT_NE(Iter, BB->end());
    auto TestCall3 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(TestCall3);
    ASSERT_EQ(Call3, TestCall3);
    ASSERT_TRUE(isa<BitCastOperator>(getCallee(TestCall3)));
    ASSERT_EQ(dyn_cast<BitCastOperator>(getCallee(TestCall3))
                      ->stripPointerCasts(),
              Fun1);
    ASSERT_EQ(TestCall3->getType(), Type::getVoidTy(Ctx));
    ASSERT_EQ(TestCall3->getNumArgOperands(), 1);
    ASSERT_TRUE(isa<ConstantInt>(TestCall3->getArgOperand(0)));
    ASSERT_EQ(TestCall3->getArgOperand(0)->getType(), Type::getInt8Ty(Ctx));
    ASSERT_EQ(
            dyn_cast<ConstantInt>(TestCall3->getArgOperand(0))->getZExtValue(),
            0);
    ++Iter;

    // The fourth call should be split into a cast and a direct call.
    ASSERT_NE(Iter, BB->end());
    auto TestCast2 = dyn_cast<CastInst>(&*Iter);
    ASSERT_TRUE(TestCast2);
    ASSERT_EQ(TestCast2->getOpcode(), Instruction::BitCast);
    ASSERT_EQ(TestCast2->getSrcTy(), Type::getInt16Ty(Ctx));
    ASSERT_EQ(TestCast2->getDestTy(), Type::getInt8Ty(Ctx));
    ASSERT_TRUE(isa<ConstantInt>(TestCast2->getOperand(0)));
    ASSERT_EQ(dyn_cast<ConstantInt>(TestCast2->getOperand(0))->getZExtValue(),
              0);
    ++Iter;
    ASSERT_NE(Iter, BB->end());
    auto TestCall4 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(TestCall4);
    ASSERT_EQ(getCallee(TestCall4), Fun2);
    ASSERT_EQ(TestCall4->getNumArgOperands(), 2);
    ASSERT_EQ(TestCall4->getArgOperand(0), TestCast2);
    ASSERT_TRUE(isa<ConstantInt>(TestCall4->getArgOperand(1)));
    ASSERT_EQ(TestCall4->getArgOperand(1)->getType(), Type::getInt8Ty(Ctx));
    ASSERT_EQ(
            dyn_cast<ConstantInt>(TestCall4->getArgOperand(1))->getZExtValue(),
            0);
    ++Iter;

    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
    ++Iter;
    ASSERT_EQ(Iter, BB->end());
}
