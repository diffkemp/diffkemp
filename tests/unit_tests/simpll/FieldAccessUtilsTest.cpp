//===---------------- FieldAccessUtilsTest.cpp - Unit tests ----------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for utility functions for working with field
/// access operations.
///
//===----------------------------------------------------------------------===//

#include "FieldAccessUtils.h"
#include <gtest/gtest.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>

/// Test fixture providing a module, a context and a function with a basic block
/// for the purpose of testing field access utility functions.
class FieldAccessUtilsTest : public ::testing::Test {
  public:
    LLVMContext Ctx;
    Module Mod{"testmod", Ctx};
    Function *Fun;
    BasicBlock *BB;

    // Composite types used for testing.
    StructType *StrTy1, *StrTy2;
    ArrayType *ArrTy;

    FieldAccessUtilsTest() : ::testing::Test() {
        Fun = Function::Create(
                FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                GlobalValue::ExternalLinkage,
                "testfun",
                &Mod);
        BB = BasicBlock::Create(Ctx, "", Fun);

        // Create two structure types for the purpose of creating GEPs with
        // the first being a member of the second.
        StrTy1 = StructType::create(
                Ctx, {Type::getInt8Ty(Ctx), Type::getInt16Ty(Ctx)}, "Str1");
        StrTy2 =
                StructType::create(Ctx, {Type::getInt8Ty(Ctx), StrTy1}, "Str2");

        // Create array type for testing non-constant indices.
        ArrTy = ArrayType::get(Type::getInt8Ty(Ctx), 10);
    }

    /// Create an inttoptr to serve as the initial pointer for a field access
    /// operation.
    IntToPtrInst *getBase(Type *Ty) {
        auto Result =
                CastInst::Create(Instruction::CastOps::IntToPtr,
                                 ConstantInt::get(Type::getInt64Ty(Ctx), 0),
                                 PointerType::get(Ty, 0),
                                 "",
                                 BB);
        return dyn_cast<IntToPtrInst>(Result);
    }
};

#define CREATE_GEP_CHAIN                                                       \
    auto Base = getBase(StrTy2);                                               \
    auto GEP1 = GetElementPtrInst::Create(                                     \
            StrTy2,                                                            \
            Base,                                                              \
            {ConstantInt::get(Type::getInt32Ty(Ctx), 0),                       \
             ConstantInt::get(Type::getInt32Ty(Ctx), 1)},                      \
            "",                                                                \
            BB);                                                               \
    auto GEP2 = GetElementPtrInst::Create(                                     \
            StrTy1,                                                            \
            GEP1,                                                              \
            {ConstantInt::get(Type::getInt32Ty(Ctx), 0),                       \
             ConstantInt::get(Type::getInt32Ty(Ctx), 0)},                      \
            "",                                                                \
            BB);                                                               \
    auto Cast = CastInst::Create(Instruction::CastOps::BitCast,                \
                                 GEP2,                                         \
                                 PointerType::get(Type::getInt16Ty(Ctx), 0),   \
                                 "",                                           \
                                 BB);

TEST_F(FieldAccessUtilsTest, GetFieldAccessStartOneGEP) {
    auto Base = getBase(StrTy2);
    auto GEP = GetElementPtrInst::Create(
            StrTy2,
            Base,
            {ConstantInt::get(Type::getInt32Ty(Ctx), 0),
             ConstantInt::get(Type::getInt32Ty(Ctx), 1),
             ConstantInt::get(Type::getInt32Ty(Ctx), 0)},
            "",
            BB);

    ASSERT_EQ(GEP, getFieldAccessStart(GEP));
}

TEST_F(FieldAccessUtilsTest, GetFieldAccessStartTwoGepsAndCast) {
    CREATE_GEP_CHAIN;

    ASSERT_EQ(GEP1, getFieldAccessStart(Cast));
    ASSERT_EQ(GEP1, getFieldAccessStart(GEP2));
}

TEST_F(FieldAccessUtilsTest, IsConstantMemoryAccessToPtr) {
    CREATE_GEP_CHAIN;
    int offset;
    bool result;

    result = isConstantMemoryAccessToPtr(GEP1, Base, offset);
    ASSERT_TRUE(result);
    ASSERT_EQ(offset, 2);
    result = isConstantMemoryAccessToPtr(GEP2, GEP1, offset);
    ASSERT_TRUE(result);
    ASSERT_EQ(offset, 0);
}

TEST_F(FieldAccessUtilsTest, IsConstantMemoryAccessToPtrNonConstIndex) {
    auto Base = getBase(ArrTy);
    auto Idx = BinaryOperator::CreateAdd(
            ConstantInt::get(Type::getInt32Ty(Ctx), 0),
            ConstantInt::get(Type::getInt32Ty(Ctx), 0),
            "",
            BB);
    auto GEP = GetElementPtrInst::Create(
            ArrTy,
            Base,
            {ConstantInt::get(Type::getInt32Ty(Ctx), 0), Idx},
            "",
            BB);

    int offset;
    bool result = isConstantMemoryAccessToPtr(GEP, Base, offset);
    ASSERT_FALSE(result);
}

TEST_F(FieldAccessUtilsTest, IsFollowingFieldAccessInstruction) {
    CREATE_GEP_CHAIN;

    ASSERT_TRUE(isFollowingFieldAccessInstruction(GEP2, GEP1));
    ASSERT_TRUE(isFollowingFieldAccessInstruction(Cast, GEP2));
    ASSERT_FALSE(isFollowingFieldAccessInstruction(Cast, GEP1));
}

TEST_F(FieldAccessUtilsTest, GetFieldAccessSourceTypes) {
    CREATE_GEP_CHAIN;
    ReturnInst::Create(Ctx, BB);
    auto Result = getFieldAccessSourceTypes(GEP1);

    std::vector<Type *> ExpectedResult{StrTy2, StrTy1};
    ASSERT_EQ(Result, ExpectedResult);
}
