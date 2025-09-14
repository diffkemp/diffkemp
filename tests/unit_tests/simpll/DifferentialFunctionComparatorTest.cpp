//===--------- DifferentialFunctionComparatorTest.cpp - Unit tests ---------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the DifferentialFunctionComparator class,
/// along with necessary classes and fixtures used by them.
///
//===----------------------------------------------------------------------===//

#include "DifferentialFunctionComparatorTest.h"
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>

/// Methods of TestComparator used to expose protected methods in
/// DifferentialFunctionComparator.

int TestComparator::testCompareSignature(bool keepSN) {
    if (!keepSN)
        beginCompare();
    return compareSignature();
}

int TestComparator::testCmpAttrs(const AttributeList L,
                                 const AttributeList R,
                                 bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpAttrs(L, R);
}

int TestComparator::testCmpAllocs(const CallInst *CL,
                                  const CallInst *CR,
                                  bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpAllocs(CL, CR);
}

int TestComparator::testCmpConstants(const Constant *CL,
                                     const Constant *CR,
                                     bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpConstants(CL, CR);
}

int TestComparator::testCmpMemset(const CallInst *CL,
                                  const CallInst *CR,
                                  bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpMemset(CL, CR);
}

int TestComparator::testCmpCallsWithExtraArg(const CallInst *CL,
                                             const CallInst *CR,
                                             bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpCallsWithExtraArg(CL, CR);
}

int TestComparator::testCmpBasicBlocks(BasicBlock *BBL,
                                       BasicBlock *BBR,
                                       bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpBasicBlocks(BBL, BBR);
}

int TestComparator::testCmpGEPs(const GEPOperator *GEPL,
                                const GEPOperator *GEPR,
                                bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpGEPs(GEPL, GEPR);
}

int TestComparator::testCmpGlobalValues(GlobalValue *L,
                                        GlobalValue *R,
                                        bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpGlobalValues(L, R);
}

int TestComparator::testCmpValues(const Value *L, const Value *R, bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpValues(L, R);
}

int TestComparator::testCmpOperations(const Instruction *L,
                                      const Instruction *R,
                                      bool &needToCmpOperands,
                                      bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpOperations(L, R, needToCmpOperands);
}

int TestComparator::testCmpOperationsWithOperands(const Instruction *L,
                                                  const Instruction *R,
                                                  bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpOperationsWithOperands(L, R);
}

int TestComparator::testCmpTypes(Type *TyL, Type *TyR, bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpTypes(TyL, TyR);
}

int TestComparator::testCmpFieldAccess(BasicBlock::const_iterator &InstL,
                                       BasicBlock::const_iterator &InstR,
                                       bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpFieldAccess(InstL, InstR);
}

int TestComparator::testCmpPHIs(PHINode *PhiL, PHINode *PhiR, bool keepSN) {
    if (!keepSN)
        beginCompare();
    return cmpPHIs(PhiL, PhiR);
}

void TestComparator::setLeftSerialNumber(const Value *Val, int i) {
    sn_mapL[Val] = i;
}

void TestComparator::setRightSerialNumber(const Value *Val, int i) {
    sn_mapR[Val] = i;
}

size_t TestComparator::getLeftSnMapSize() { return sn_mapL.size(); }
size_t TestComparator::getRightSnMapSize() { return sn_mapR.size(); }

/// Extend the set of custom patterns.
void TestComparator::addCustomPatternSet(const CustomPatternSet *PatternSet) {
    CustomPatternComp.addPatternSet(PatternSet, FnL, FnR);
}

/// Methods of DifferentialFunctionComparatorTest fixture used for setup of
/// unit tests.

/// Initialise functions to be tested (FL, FR)
/// and prepares DifferentialFunctionComparator.
DifferentialFunctionComparatorTest::DifferentialFunctionComparatorTest()
        : ::testing::Test() {

// From LLVM 19 there is a new format of debug info, which we do not currently
// support.
#if LLVM_VERSION_MAJOR >= 19
    ModL->convertFromNewDbgValues();
    ModR->convertFromNewDbgValues();
#endif

    // Create one function in each module for testing purposes.
    FL = Function::Create(FunctionType::get(Type::getVoidTy(CtxL), {}, false),
                          GlobalValue::ExternalLinkage,
                          "F",
                          ModL.get());
    FR = Function::Create(FunctionType::get(Type::getVoidTy(CtxR), {}, false),
                          GlobalValue::ExternalLinkage,
                          "F",
                          ModR.get());
    prepareDFC();
}

/// Prepares DifferentialFunctionComparator.
void DifferentialFunctionComparatorTest::prepareDFC() {
    // Create the DebugInfo object and a ModuleComparator.
    // Note: DifferentialFunctionComparator cannot function without
    // ModuleComparator and DebugInfo.
    DbgInfo = std::make_unique<DebugInfo>(
            *ModL, *ModR, FL, FR, CalledFirst, CalledSecond, Conf.Patterns);
    ModComp = std::make_unique<ModuleComparator>(*ModL,
                                                 *ModR,
                                                 Conf,
                                                 DbgInfo.get(),
                                                 StructSizeMapL,
                                                 StructSizeMapR,
                                                 StructDIMapL,
                                                 StructDIMapR);
    // Add function pair to ComparedFuns.
    // Note: even though ModuleComparator is not tested here,
    // DifferentialFunctionComparator expects the presence of the key in
    // the map, therefore it is necessary to do this here.
    ModComp->ComparedFuns.emplace(std::make_pair(FL, FR), Result{});

    // Generate debug metadata.
    generateDebugMetadata();

    // Finally create the comparator.
    DiffComp = std::make_unique<TestComparator>(FL,
                                                FR,
                                                Conf,
                                                DbgInfo.get(),
                                                &ModComp.get()->CustomPatterns,
                                                ModComp.get());
}

/// Generates a file, compile unit and subprogram for each module.
void DifferentialFunctionComparatorTest::generateDebugMetadata(
        DICompositeTypeArray DTyArrL,
        DICompositeTypeArray DTyArrR,
        DIMacroNodeArray DMacArrL,
        DIMacroNodeArray DMacArrR) {

    DIBuilder builderL(*ModL);
    DIFile *DScoL = builderL.createFile("test", "test");
    DICompileUnit *DCUL =
            builderL.createCompileUnit(0, DScoL, "test", false, "", 0);
    DSubL = builderL.createFunction(DCUL, "test", "test", DScoL, 1, nullptr, 1);
    builderL.finalizeSubprogram(DSubL);

    DIBuilder builderR(*ModR);
    DIFile *DScoR = builderR.createFile("test", "test");
    DICompileUnit *DCUR =
            builderR.createCompileUnit(0, DScoR, "test", false, "", 0);
    DSubR = builderR.createFunction(DCUR, "test", "test", DScoR, 1, nullptr, 1);
    builderL.finalizeSubprogram(DSubR);
}

/// Compares two functions using cmpGlobalValues called through
/// cmpBasicBlocks on a pair of auxilliary basic blocks containing calls
/// to the functions.
int DifferentialFunctionComparatorTest::testFunctionComparison(Function *FunL,
                                                               Function *FunR) {
    const std::string auxFunName = "AuxFunComp";

    // Testing function comparison is a little bit tricky, because for the
    // callee generation the call location must be set at the time the
    // comparison is done.
    // To ensure this a pair of auxilliary functions containing a call to
    // the functions is added, along with their locations.
    if (auto OldFun = ModL->getFunction(auxFunName)) {
        OldFun->eraseFromParent();
    }
    if (auto OldFun = ModR->getFunction(auxFunName)) {
        OldFun->eraseFromParent();
    }

    Function *AuxFL = Function::Create(
            FunctionType::get(Type::getVoidTy(CtxL), {}, false),
            GlobalValue::ExternalLinkage,
            auxFunName,
            ModL.get());
    Function *AuxFR = Function::Create(
            FunctionType::get(Type::getVoidTy(CtxR), {}, false),
            GlobalValue::ExternalLinkage,
            auxFunName,
            ModR.get());
    BasicBlock *BBL = BasicBlock::Create(CtxL, "", AuxFL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", AuxFR);

    CallInst *CL = CallInst::Create(FunL->getFunctionType(), FunL, "", BBL);
    CallInst *CR = CallInst::Create(FunR->getFunctionType(), FunR, "", BBR);

    // Add debug info.
    DILocation *DLocL = DILocation::get(CtxL, 1, 1, DSubL);
    DILocation *DLocR = DILocation::get(CtxR, 1, 1, DSubR);
    CL->setDebugLoc(DebugLoc{DLocL});
    CR->setDebugLoc(DebugLoc{DLocR});

    // Finish the basic blocks with return instructions and return the
    // result of cmpBasicBlocks.
    ReturnInst::Create(CtxL, BBL);
    ReturnInst::Create(CtxR, BBR);

    return DiffComp->testCmpBasicBlocks(BBL, BBR);
}

/// Unit tests

/// Tests a comparison of two GEPs of a structure type with a constant index
/// that has to compared using debug info.
TEST_F(DifferentialFunctionComparatorTest, CmpGepsRenamed) {
    // Create structure types to test the GEPs.
    StructType *STyL =
            StructType::create({Type::getInt8Ty(CtxL), Type::getInt8Ty(CtxL)});
    STyL->setName("struct.test");
    StructType *STyR = StructType::create({Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR)});
    STyR->setName("struct.test");

    // Add entries to DebugInfo.
    // Note: attr3 is added between attr1 and attr2, causing the index shifting
    // tested here.
    std::string attr1("attr1"), attr2("attr2"), attr3("attr3");
    DbgInfo->StructFieldNames[{STyL, 0}] = attr1;
    DbgInfo->StructFieldNames[{STyL, 1}] = attr2;
    DbgInfo->StructFieldNames[{STyR, 0}] = attr1;
    DbgInfo->StructFieldNames[{STyR, 1}] = attr3;
    DbgInfo->StructFieldNames[{STyR, 2}] = attr2;

    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    AllocaInst *VarL = new AllocaInst(STyL, 0, "var", BBL);
    AllocaInst *VarR = new AllocaInst(STyR, 0, "var", BBR);
    GetElementPtrInst *GEP1L = GetElementPtrInst::Create(
            STyL,
            VarL,
            {ConstantInt::get(Type::getInt32Ty(CtxL), 0),
             ConstantInt::get(Type::getInt32Ty(CtxL), 1)},
            "",
            BBL);
    GetElementPtrInst *GEP1R = GetElementPtrInst::Create(
            STyR,
            VarR,
            {ConstantInt::get(Type::getInt32Ty(CtxR), 0),
             ConstantInt::get(Type::getInt32Ty(CtxR), 2)},
            "",
            BBR);
    GetElementPtrInst *GEP2L = GetElementPtrInst::Create(
            STyL,
            VarL,
            {ConstantInt::get(Type::getInt32Ty(CtxL), 0),
             ConstantInt::get(Type::getInt32Ty(CtxL), 0)},
            "",
            BBL);
    GetElementPtrInst *GEP2R = GetElementPtrInst::Create(
            STyR,
            VarR,
            {ConstantInt::get(Type::getInt32Ty(CtxR), 0),
             ConstantInt::get(Type::getInt32Ty(CtxR), 2)},
            "",
            BBR);

    // The structures have the same name, therefore the corresponding indices
    // should be compared as equal (while non-corresponding ones stay not
    // equal).
    ASSERT_EQ(DiffComp->testCmpGEPs(dyn_cast<GEPOperator>(GEP1L),
                                    dyn_cast<GEPOperator>(GEP1R)),
              0);
    ASSERT_EQ(DiffComp->testCmpGEPs(dyn_cast<GEPOperator>(GEP2L),
                                    dyn_cast<GEPOperator>(GEP2R)),
              1);

    // Now rename one of the structures and check whether the comparison result
    // changed.
    STyL->setName("struct.1");
    STyR->setName("struct.2");
    ASSERT_EQ(DiffComp->testCmpGEPs(dyn_cast<GEPOperator>(GEP1L),
                                    dyn_cast<GEPOperator>(GEP1R)),
              -1);
}

/// Tests attribute comparison (currently attributes are always ignored).
TEST_F(DifferentialFunctionComparatorTest, CmpAttrs) {
    AttributeList L, R;
    ASSERT_EQ(DiffComp->testCmpAttrs(L, R), 0);
}

/// Tests the comparison of calls to allocation functions.
TEST_F(DifferentialFunctionComparatorTest, CmpAllocs) {
    // Create auxilliary functions to serve as the allocation functions.
    Function *AuxFL = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxL), 0),
                              {Type::getInt32Ty(CtxL)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFL",
            ModL.get());
    Function *AuxFR = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxR), 0),
                              {Type::getInt32Ty(CtxR)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFR",
            ModR.get());

    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    // Test call instructions with the same value.
    CallInst *CL =
            CallInst::Create(AuxFL->getFunctionType(),
                             AuxFL,
                             {ConstantInt::get(Type::getInt32Ty(CtxL), 42)},
                             "",
                             BBL);
    CallInst *CR =
            CallInst::Create(AuxFR->getFunctionType(),
                             AuxFR,
                             {ConstantInt::get(Type::getInt32Ty(CtxR), 42)},
                             "",
                             BBR);

    // Create calls to llvm.dbg.value with type metadata.
    DIBuilder builderL(*ModL);
    DIBuilder builderR(*ModR);
    DIFile *UnitL = builderL.createFile("foo", "bar");
    DIFile *UnitR = builderL.createFile("foo", "bar");
    DISubprogram *FunTypeL =
            builderL.createFunction(UnitL, "F", "F", UnitL, 0, nullptr, 0);
    DISubprogram *FunTypeR =
            builderR.createFunction(UnitR, "F", "F", UnitR, 0, nullptr, 0);
    DIBasicType *PointeeTypeL = builderL.createNullPtrType();
    DIBasicType *PointeeTypeR = builderR.createNullPtrType();
    DIDerivedType *PointerTypeL = builderL.createPointerType(PointeeTypeL, 64);
    DIDerivedType *PointerTypeR = builderR.createPointerType(PointeeTypeR, 64);
    DILocalVariable *varL = builderL.createAutoVariable(
            FunTypeL, "var", nullptr, 0, PointerTypeL);
    DILocalVariable *varR = builderR.createAutoVariable(
            FunTypeR, "var", nullptr, 0, PointerTypeR);
    DIExpression *exprL = builderL.createExpression();
    DIExpression *exprR = builderR.createExpression();
    DILocation *locL = DILocation::get(DSubL->getContext(), 0, 0, DSubL);
    DILocation *locR = DILocation::get(DSubR->getContext(), 0, 0, DSubR);
    builderL.insertDbgValueIntrinsic(CL, varL, exprL, locL, BBL);
    builderR.insertDbgValueIntrinsic(CR, varR, exprR, locR, BBR);

    ASSERT_EQ(DiffComp->testCmpAllocs(CL, CR), 0);

    // Create structure types and calls for testing of allocation comparison
    // in cases where the structure size changed.
    StructType *STyL =
            StructType::create({Type::getInt8Ty(CtxL), Type::getInt8Ty(CtxL)});
    STyL->setName("struct.test");
    StructType *STyR = StructType::create({Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR)});
    STyR->setName("struct.test");
    uint64_t STyLSize = ModL->getDataLayout().getTypeStoreSize(STyL);
    uint64_t STyRSize = ModR->getDataLayout().getTypeStoreSize(STyR);
    CL = CallInst::Create(AuxFL->getFunctionType(),
                          AuxFL,
                          {ConstantInt::get(Type::getInt32Ty(CtxL), STyLSize)},
                          "",
                          BBL);
    CR = CallInst::Create(AuxFR->getFunctionType(),
                          AuxFR,
                          {ConstantInt::get(Type::getInt32Ty(CtxR), STyRSize)},
                          "",
                          BBR);

    // Add casts to allow cmpAllocs to check whether the structure types match.
#if LLVM_VERSION_MAJOR < 15
    CastInst *CastL = CastInst::CreateTruncOrBitCast(CL, STyL, "", BBL);
    CastInst *CastR = CastInst::CreateTruncOrBitCast(CR, STyR, "", BBR);
#endif
    DIBasicType *Int8TypeL =
            builderL.createBasicType("int8_t", 8, dwarf::DW_ATE_signed);
    DIBasicType *Int8TypeR =
            builderR.createBasicType("int8_t", 8, dwarf::DW_ATE_signed);
    DICompositeType *StructTypeL = builderL.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            16,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderL.getOrCreateArray({Int8TypeL, Int8TypeL}));
    DICompositeType *StructTypeR = builderR.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            24,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderR.getOrCreateArray({Int8TypeR, Int8TypeR, Int8TypeR}));

    // Create calls to llvm.dbg.value with type metadata.
    PointerTypeL = builderL.createPointerType(StructTypeL, 64);
    PointerTypeR = builderR.createPointerType(StructTypeR, 64);
    varL = builderL.createAutoVariable(
            FunTypeL, "var", nullptr, 0, PointerTypeL);
    varR = builderR.createAutoVariable(
            FunTypeR, "var", nullptr, 0, PointerTypeR);
    exprL = builderL.createExpression();
    exprR = builderR.createExpression();
    locL = DILocation::get(DSubL->getContext(), 0, 0, DSubL);
    locR = DILocation::get(DSubR->getContext(), 0, 0, DSubR);
    builderL.insertDbgValueIntrinsic(CL, varL, exprL, locL, BBL);
    builderR.insertDbgValueIntrinsic(CR, varR, exprR, locR, BBR);
    ASSERT_EQ(DiffComp->testCmpAllocs(CL, CR), 0);

    // Repeat the test again, but now with different structure types.
    StructType *STyR2 = StructType::create({Type::getInt8Ty(CtxR),
                                            Type::getInt8Ty(CtxR),
                                            Type::getInt8Ty(CtxR)});
    STyR2->setName("struct.test2");
    uint64_t STyR2Size = ModR->getDataLayout().getTypeStoreSize(STyR2);
    CR = CallInst::Create(AuxFR->getFunctionType(),
                          AuxFR,
                          {ConstantInt::get(Type::getInt32Ty(CtxR), STyR2Size)},
                          "",
                          BBR);
#if LLVM_VERSION_MAJOR < 15
    CastR = CastInst::CreateTruncOrBitCast(CR, STyR2, "", BBR);
#endif

    // Create calls to llvm.dbg.value with type metadata.
    StructTypeR = builderR.createStructType(
            nullptr,
            "struct.test2",
            nullptr,
            0,
            24,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderR.getOrCreateArray({Int8TypeR, Int8TypeR, Int8TypeR}));
    PointerTypeR = builderR.createPointerType(StructTypeR, 64);
    varR = builderR.createAutoVariable(
            FunTypeR, "var", nullptr, 0, PointerTypeR);
    builderR.insertDbgValueIntrinsic(CR, varR, exprR, locR, BBR);
    ASSERT_EQ(DiffComp->testCmpAllocs(CL, CR), 1);
}

/// Tests the comparison of calls to memset functions.
TEST_F(DifferentialFunctionComparatorTest, CmpMemsets) {
    // Create auxilliary functions to serve as the memset functions.
    Function *AuxFL = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxL), 0),
                              {PointerType::get(Type::getVoidTy(CtxL), 0),
                               Type::getInt32Ty(CtxL),
                               Type::getInt32Ty(CtxL)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFL",
            ModL.get());
    Function *AuxFR = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxR), 0),
                              {PointerType::get(Type::getVoidTy(CtxR), 0),
                               Type::getInt32Ty(CtxR),
                               Type::getInt32Ty(CtxR)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFR",
            ModR.get());

    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    // Create structure types and allocas that will be used by the memset calls.
    StructType *STyL =
            StructType::create({Type::getInt8Ty(CtxL), Type::getInt8Ty(CtxL)});
    STyL->setName("struct.test");
    StructType *STyR = StructType::create({Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR)});
    STyR->setName("struct.test");
    uint64_t STyLSize = ModL->getDataLayout().getTypeStoreSize(STyL);
    uint64_t STyRSize = ModR->getDataLayout().getTypeStoreSize(STyR);
    AllocaInst *AllL = new AllocaInst(STyL, 0, "var", BBL);
    AllocaInst *AllR = new AllocaInst(STyR, 0, "var", BBR);

    // First test two memsets that differ in value that is set.
    CallInst *CL = CallInst::Create(
            AuxFL->getFunctionType(),
            AuxFL,
            {AllL,
             ConstantInt::get(Type::getInt32Ty(CtxL), 5),
             ConstantInt::get(Type::getInt32Ty(CtxL), STyLSize)},
            "",
            BBL);
    CallInst *CR = CallInst::Create(
            AuxFR->getFunctionType(),
            AuxFR,
            {AllR,
             ConstantInt::get(Type::getInt32Ty(CtxR), 6),
             ConstantInt::get(Type::getInt32Ty(CtxR), STyRSize)},
            "",
            BBR);

    // Create calls to llvm.dbg.value with type metadata.
    DIBuilder builderL(*ModL);
    DIBuilder builderR(*ModR);
    DIFile *UnitL = builderL.createFile("foo", "bar");
    DIFile *UnitR = builderL.createFile("foo", "bar");
    DISubprogram *FunTypeL =
            builderL.createFunction(UnitL, "F", "F", UnitL, 0, nullptr, 0);
    DISubprogram *FunTypeR =
            builderR.createFunction(UnitR, "F", "F", UnitR, 0, nullptr, 0);
    auto Int8TypeL =
            builderL.createBasicType("int8_t", 8, dwarf::DW_ATE_signed);
    auto Int8TypeR =
            builderR.createBasicType("int8_t", 8, dwarf::DW_ATE_signed);
    auto StructTypeL = builderL.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            STyLSize * 8,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderL.getOrCreateArray({Int8TypeL, Int8TypeL}));
    auto StructTypeR = builderR.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            STyRSize * 8,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderR.getOrCreateArray({Int8TypeR, Int8TypeR, Int8TypeR}));
    auto PointerTypeL = builderL.createPointerType(StructTypeL, 64);
    auto PointerTypeR = builderR.createPointerType(StructTypeR, 64);
    DILocalVariable *varL = builderL.createAutoVariable(
            FunTypeL, "var", nullptr, 0, PointerTypeL);
    DILocalVariable *varR = builderR.createAutoVariable(
            FunTypeR, "var", nullptr, 0, PointerTypeR);
    DIExpression *exprL = builderL.createExpression();
    DIExpression *exprR = builderR.createExpression();
    DILocation *locL = DILocation::get(DSubL->getContext(), 0, 0, DSubL);
    DILocation *locR = DILocation::get(DSubR->getContext(), 0, 0, DSubR);
    builderL.insertDbgValueIntrinsic(AllL, varL, exprL, locL, BBL);
    builderR.insertDbgValueIntrinsic(AllR, varR, exprR, locR, BBR);

    ASSERT_EQ(DiffComp->testCmpMemset(CL, CR), -1);

    // Then test a case when the set value is the same and the arguments differ
    // only in the structure size.
    CL = CallInst::Create(AuxFL->getFunctionType(),
                          AuxFL,
                          {AllL,
                           ConstantInt::get(Type::getInt32Ty(CtxL), 5),
                           ConstantInt::get(Type::getInt32Ty(CtxL), STyLSize)},
                          "",
                          BBL);
    CR = CallInst::Create(AuxFR->getFunctionType(),
                          AuxFR,
                          {AllR,
                           ConstantInt::get(Type::getInt32Ty(CtxR), 5),
                           ConstantInt::get(Type::getInt32Ty(CtxR), STyRSize)},
                          "",
                          BBR);
    builderL.insertDbgValueIntrinsic(AllL, varL, exprL, locL, BBL);
    builderR.insertDbgValueIntrinsic(AllR, varR, exprR, locR, BBR);
    ASSERT_EQ(DiffComp->testCmpMemset(CL, CR), 0);
}

/// Tests the comparison of calls to memset functions.
/// Example when the compiled LLVM IR has multiple debug metadata describing
/// memset destination variable. The first one describes the variable in scope
/// of current function (contains info about the type on which the variable
/// points to). The second debug info is from the scope of stdlib C memset
/// function which was 'inlined' and does not contain info about the pointee
/// type. The multiple debug metadata for the same variable caused problems for
/// LLVM >= 15, where we used dbg info for extracting the pointee type info.
TEST_F(DifferentialFunctionComparatorTest, CmpMemsetsMultipleDebugMetadata) {
    // Create auxilliary functions to serve as the memset functions.
    Function *AuxFL = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxL), 0),
                              {PointerType::get(Type::getVoidTy(CtxL), 0),
                               Type::getInt32Ty(CtxL),
                               Type::getInt32Ty(CtxL)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFL",
            ModL.get());
    Function *AuxFR = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxR), 0),
                              {PointerType::get(Type::getVoidTy(CtxR), 0),
                               Type::getInt32Ty(CtxR),
                               Type::getInt32Ty(CtxR)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFR",
            ModR.get());

    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    // Create structure types and allocas that will be used by the memset calls.
    StructType *STyL =
            StructType::create({Type::getInt8Ty(CtxL), Type::getInt8Ty(CtxL)});
    STyL->setName("struct.test");
    StructType *STyR = StructType::create({Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR)});
    STyR->setName("struct.test");
    uint64_t STyLSize = ModL->getDataLayout().getTypeStoreSize(STyL);
    uint64_t STyRSize = ModR->getDataLayout().getTypeStoreSize(STyR);
    AllocaInst *AllL = new AllocaInst(STyL, 0, "var", BBL);
    AllocaInst *AllR = new AllocaInst(STyR, 0, "var", BBR);

    CallInst *CL = CallInst::Create(
            AuxFL->getFunctionType(),
            AuxFL,
            {AllL,
             ConstantInt::get(Type::getInt32Ty(CtxL), 5),
             ConstantInt::get(Type::getInt32Ty(CtxL), STyLSize)},
            "",
            BBL);
    CallInst *CR = CallInst::Create(
            AuxFR->getFunctionType(),
            AuxFR,
            {AllR,
             ConstantInt::get(Type::getInt32Ty(CtxR), 5),
             ConstantInt::get(Type::getInt32Ty(CtxR), STyRSize)},
            "",
            BBR);

    // Debug metadata describing var from the scope of current (F) function.
    DIBuilder builderL(*ModL);
    DIBuilder builderR(*ModR);
    DIFile *UnitL = builderL.createFile("foo", "bar");
    DIFile *UnitR = builderL.createFile("foo", "bar");
    DISubprogram *FunTypeL =
            builderL.createFunction(UnitL, "F", "F", UnitL, 0, nullptr, 0);
    DISubprogram *FunTypeR =
            builderR.createFunction(UnitR, "F", "F", UnitR, 0, nullptr, 0);
    auto Int8TypeL =
            builderL.createBasicType("int8_t", 8, dwarf::DW_ATE_signed);
    auto Int8TypeR =
            builderR.createBasicType("int8_t", 8, dwarf::DW_ATE_signed);
    auto StructTypeL = builderL.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            STyLSize * 8,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderL.getOrCreateArray({Int8TypeL, Int8TypeL}));
    auto StructTypeR = builderR.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            STyRSize * 8,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderR.getOrCreateArray({Int8TypeR, Int8TypeR, Int8TypeR}));
    auto PointerTypeL = builderL.createPointerType(StructTypeL, 64);
    auto PointerTypeR = builderR.createPointerType(StructTypeR, 64);
    DILocalVariable *varL = builderL.createAutoVariable(
            FunTypeL, "var", nullptr, 0, PointerTypeL);
    DILocalVariable *varR = builderR.createAutoVariable(
            FunTypeR, "var", nullptr, 0, PointerTypeR);
    DIExpression *exprL = builderL.createExpression();
    DIExpression *exprR = builderR.createExpression();
    DILocation *locL = DILocation::get(DSubL->getContext(), 0, 0, DSubL);
    DILocation *locR = DILocation::get(DSubR->getContext(), 0, 0, DSubR);
    builderL.insertDbgValueIntrinsic(AllL, varL, exprL, locL, BBL);
    builderR.insertDbgValueIntrinsic(AllR, varR, exprR, locR, BBR);

    // Debug metadata describing var from the scope of memset function.
    DIFile *MemsetUnitL = builderL.createFile("memset", "stdlib");
    DIFile *MemsetUnitR = builderL.createFile("memset", "stdlib");
    DISubprogram *MemsetTypeL = builderL.createFunction(
            UnitL, "memset", "memset", MemsetUnitL, 0, nullptr, 0);
    DISubprogram *MemsetTypeR = builderR.createFunction(
            UnitR, "memset", "memset", MemsetUnitR, 0, nullptr, 0);
    auto MemsetPointerTypeL = builderL.createPointerType(nullptr, 64);
    auto MemsetPointerTypeR = builderR.createPointerType(nullptr, 64);
    DILocalVariable *memsetVarL = builderL.createAutoVariable(
            MemsetTypeL, "__dest", nullptr, 0, MemsetPointerTypeL);
    DILocalVariable *memsetVarR = builderR.createAutoVariable(
            MemsetTypeR, "__dest", nullptr, 0, MemsetPointerTypeR);
    builderL.insertDbgValueIntrinsic(
            AllL, memsetVarL, builderL.createExpression(), locL, BBL);
    builderR.insertDbgValueIntrinsic(
            AllR, memsetVarR, builderR.createExpression(), locR, BBR);

    ASSERT_EQ(DiffComp->testCmpMemset(CL, CR), 0);
}

/// Tests the comparison of calls to memset functions with void * type
/// (compiled as i8*) and different sizes. The result should be non-equal
/// because we do not have enough information (type name and size) to evaluate
/// the void* types as equal. This used to caused errors for LLVM >= 15.
TEST_F(DifferentialFunctionComparatorTest, CmpMemsetsVoidPtrType) {
    // Create auxilliary functions to serve as the memset functions.
    Function *AuxFL = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxL), 0),
                              {PointerType::get(Type::getVoidTy(CtxL), 0),
                               Type::getInt32Ty(CtxL),
                               Type::getInt32Ty(CtxL)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFL",
            ModL.get());
    Function *AuxFR = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxR), 0),
                              {PointerType::get(Type::getVoidTy(CtxR), 0),
                               Type::getInt32Ty(CtxR),
                               Type::getInt32Ty(CtxR)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFR",
            ModR.get());

    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    AllocaInst *AllL = new AllocaInst(Type::getInt8Ty(CtxL), 0, "var", BBL);
    AllocaInst *AllR = new AllocaInst(Type::getInt8Ty(CtxR), 0, "var", BBR);

    CallInst *CL =
            CallInst::Create(AuxFL->getFunctionType(),
                             AuxFL,
                             {AllL,
                              ConstantInt::get(Type::getInt32Ty(CtxL), 5),
                              ConstantInt::get(Type::getInt32Ty(CtxL), 8)},
                             "",
                             BBL);
    CallInst *CR =
            CallInst::Create(AuxFR->getFunctionType(),
                             AuxFR,
                             {AllR,
                              ConstantInt::get(Type::getInt32Ty(CtxR), 5),
                              ConstantInt::get(Type::getInt32Ty(CtxR), 12)},
                             "",
                             BBR);

    // Create calls to llvm.dbg.value with type metadata.
    DIBuilder builderL(*ModL);
    DIBuilder builderR(*ModR);
    DIFile *UnitL = builderL.createFile("foo", "bar");
    DIFile *UnitR = builderL.createFile("foo", "bar");
    DISubprogram *FunTypeL =
            builderL.createFunction(UnitL, "F", "F", UnitL, 0, nullptr, 0);
    DISubprogram *FunTypeR =
            builderR.createFunction(UnitR, "F", "F", UnitR, 0, nullptr, 0);
    // Void * type has nullptr pointee type.
    auto PointerTypeL = builderL.createPointerType(nullptr, 64);
    auto PointerTypeR = builderR.createPointerType(nullptr, 64);
    DILocalVariable *varL = builderL.createAutoVariable(
            FunTypeL, "var", nullptr, 0, PointerTypeL);
    DILocalVariable *varR = builderR.createAutoVariable(
            FunTypeR, "var", nullptr, 0, PointerTypeR);
    DIExpression *exprL = builderL.createExpression();
    DIExpression *exprR = builderR.createExpression();
    DILocation *locL = DILocation::get(DSubL->getContext(), 0, 0, DSubL);
    DILocation *locR = DILocation::get(DSubR->getContext(), 0, 0, DSubR);
    builderL.insertDbgValueIntrinsic(AllL, varL, exprL, locL, BBL);
    builderR.insertDbgValueIntrinsic(AllR, varR, exprR, locR, BBR);

    ASSERT_NE(DiffComp->testCmpMemset(CL, CR), 0);
}

/// Tests the comparison of calls to memset functions called with pointer to
/// typedefed struct, this caused problems for opaque pointers.
TEST_F(DifferentialFunctionComparatorTest, CmpMemsetsOfTypedef) {
    // // old version
    // typedef struct test {
    //   char a;
    //   long b;
    //   char c;
    // } s;
    // void F(s *var) {
    //   memset(var, 0, sizeof(s));
    // }
    // // new version - better alignment of the struct = has smaller size
    // typedef struct test {
    //   char a;
    //   char c;
    //   long b;
    // } s;
    // void F(s *var) {
    //   memset(var, 0, sizeof(s));
    // }

    // Create auxilliary functions to serve as the memset functions.
    Function *AuxFL = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxL), 0),
                              {PointerType::get(Type::getVoidTy(CtxL), 0),
                               Type::getInt32Ty(CtxL),
                               Type::getInt32Ty(CtxL)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFL",
            ModL.get());
    Function *AuxFR = Function::Create(
            FunctionType::get(PointerType::get(Type::getVoidTy(CtxR), 0),
                              {PointerType::get(Type::getVoidTy(CtxR), 0),
                               Type::getInt32Ty(CtxR),
                               Type::getInt32Ty(CtxR)},
                              false),
            GlobalValue::ExternalLinkage,
            "AuxFR",
            ModR.get());

    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    // Create structure types and allocas that will be used by the memset calls.
    StructType *STyL = StructType::create({Type::getInt8Ty(CtxL),
                                           Type::getInt64Ty(CtxL),
                                           Type::getInt8Ty(CtxL)});
    STyL->setName("struct.test");
    StructType *STyR = StructType::create({Type::getInt8Ty(CtxR),
                                           Type::getInt8Ty(CtxR),
                                           Type::getInt64Ty(CtxR)});
    STyR->setName("struct.test");
    // The sizes are different because of swapped struct fields causing
    // different alignment and padding.
    uint64_t STyLSize = ModL->getDataLayout().getTypeStoreSize(STyL);
    uint64_t STyRSize = ModR->getDataLayout().getTypeStoreSize(STyR);
    AllocaInst *AllL = new AllocaInst(STyL, 0, "var", BBL);
    AllocaInst *AllR = new AllocaInst(STyR, 0, "var", BBR);

    CallInst *CL = CallInst::Create(
            AuxFL->getFunctionType(),
            AuxFL,
            {AllL,
             ConstantInt::get(Type::getInt32Ty(CtxL), 0),
             ConstantInt::get(Type::getInt32Ty(CtxL), STyLSize)},
            "",
            BBL);
    CallInst *CR = CallInst::Create(
            AuxFR->getFunctionType(),
            AuxFR,
            {AllR,
             ConstantInt::get(Type::getInt32Ty(CtxR), 0),
             ConstantInt::get(Type::getInt32Ty(CtxR), STyRSize)},
            "",
            BBR);

    // Create calls to llvm.dbg.value with type metadata.
    DIBuilder builderL(*ModL);
    DIBuilder builderR(*ModR);
    DIFile *UnitL = builderL.createFile("foo", "bar");
    DIFile *UnitR = builderL.createFile("foo", "bar");
    DISubprogram *FunTypeL =
            builderL.createFunction(UnitL, "F", "F", UnitL, 0, nullptr, 0);
    DISubprogram *FunTypeR =
            builderR.createFunction(UnitR, "F", "F", UnitR, 0, nullptr, 0);
    auto Int8TypeL =
            builderL.createBasicType("int8_t", 8, dwarf::DW_ATE_signed_char);
    auto Int8TypeR =
            builderR.createBasicType("int8_t", 8, dwarf::DW_ATE_signed_char);
    auto Int64TypeL =
            builderL.createBasicType("int64_t", 8, dwarf::DW_ATE_signed);
    auto Int64TypeR =
            builderR.createBasicType("int64_t", 8, dwarf::DW_ATE_signed);
    auto StructTypeL = builderL.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            STyLSize * 8,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderL.getOrCreateArray({Int8TypeL, Int64TypeL, Int8TypeL}));
    auto StructTypeR = builderR.createStructType(
            nullptr,
            "struct.test",
            nullptr,
            0,
            STyRSize * 8,
            0,
            static_cast<DINode::DIFlags>(0),
            nullptr,
            builderR.getOrCreateArray({Int8TypeR, Int8TypeR, Int64TypeR}));
    auto TypedefL = builderL.createTypedef(StructTypeL, "s", UnitL, 0, nullptr);
    auto TypedefR = builderL.createTypedef(StructTypeR, "s", UnitR, 0, nullptr);
    auto PointerTypeL = builderL.createPointerType(TypedefL, 64);
    auto PointerTypeR = builderR.createPointerType(TypedefR, 64);
    DILocalVariable *varL = builderL.createAutoVariable(
            FunTypeL, "var", nullptr, 0, PointerTypeL);
    DILocalVariable *varR = builderR.createAutoVariable(
            FunTypeR, "var", nullptr, 0, PointerTypeR);
    DIExpression *exprL = builderL.createExpression();
    DIExpression *exprR = builderR.createExpression();
    DILocation *locL = DILocation::get(DSubL->getContext(), 0, 0, DSubL);
    DILocation *locR = DILocation::get(DSubR->getContext(), 0, 0, DSubR);
    builderL.insertDbgValueIntrinsic(AllL, varL, exprL, locL, BBL);
    builderR.insertDbgValueIntrinsic(AllR, varR, exprR, locR, BBR);

    ASSERT_EQ(DiffComp->testCmpMemset(CL, CR), 0);
}

/// Tests several cases where cmpTypes should detect a semantic equivalence.
TEST_F(DifferentialFunctionComparatorTest, CmpTypes) {
    // Try to compare a union type of a greater size than the other type.
    StructType *STyL = StructType::create({Type::getInt32Ty(CtxL)});
    Type *IntTyR = Type::getInt16Ty(CtxL);
    STyL->setName("union.test");
    ASSERT_EQ(DiffComp->testCmpTypes(STyL, IntTyR), 0);
    ASSERT_EQ(DiffComp->testCmpTypes(IntTyR, STyL), 0);
    // Rename the type to remove "union" from the name and check the result
    // again.
    STyL->setName("struct.test");
    ASSERT_EQ(DiffComp->testCmpTypes(STyL, IntTyR), 1);
    ASSERT_EQ(DiffComp->testCmpTypes(IntTyR, STyL), -1);

    // Then try to compare a union type of smaller size that the other type.
    STyL = StructType::create({Type::getInt16Ty(CtxL)});
    IntTyR = Type::getInt32Ty(CtxL);
    STyL->setName("union.test");
    ASSERT_EQ(DiffComp->testCmpTypes(STyL, IntTyR), 1);
    ASSERT_EQ(DiffComp->testCmpTypes(IntTyR, STyL), -1);

    // Integer types and array times with the same element type should compare
    // as equivalent when ignoring type casts.
    ASSERT_EQ(DiffComp->testCmpTypes(Type::getInt16Ty(CtxL),
                                     Type::getInt8Ty(CtxR)),
              1);
    ASSERT_EQ(DiffComp->testCmpTypes(ArrayType::get(Type::getInt8Ty(CtxL), 10),
                                     ArrayType::get(Type::getInt8Ty(CtxR), 11)),
              -1);
    Conf.Patterns.TypeCasts = true;
    ASSERT_EQ(DiffComp->testCmpTypes(Type::getInt16Ty(CtxL),
                                     Type::getInt8Ty(CtxR)),
              0);
    ASSERT_EQ(DiffComp->testCmpTypes(ArrayType::get(Type::getInt8Ty(CtxL), 10),
                                     ArrayType::get(Type::getInt8Ty(CtxR), 11)),
              0);
    // Boolean type should stay unequal.
    ASSERT_EQ(DiffComp->testCmpTypes(ArrayType::get(Type::getInt1Ty(CtxL), 10),
                                     ArrayType::get(Type::getInt8Ty(CtxR), 11)),
              1);
}

/// Tests the comparison of constant global variables using cmpGlobalValues.
TEST_F(DifferentialFunctionComparatorTest, CmpGlobalValuesConstGlobalVars) {
    GlobalVariable *GVL1 =
            new GlobalVariable(*ModL,
                               Type::getInt8Ty(CtxL),
                               true,
                               GlobalValue::ExternalLinkage,
                               ConstantInt::get(Type::getInt32Ty(CtxL), 6));
    GlobalVariable *GVR1 =
            new GlobalVariable(*ModR,
                               Type::getInt8Ty(CtxR),
                               true,
                               GlobalValue::ExternalLinkage,
                               ConstantInt::get(Type::getInt32Ty(CtxR), 6));
    GlobalVariable *GVR2 =
            new GlobalVariable(*ModR,
                               Type::getInt8Ty(CtxR),
                               true,
                               GlobalValue::ExternalLinkage,
                               ConstantInt::get(Type::getInt32Ty(CtxR), 5));

    ASSERT_EQ(DiffComp->testCmpGlobalValues(GVL1, GVR1), 0);
    ASSERT_EQ(DiffComp->testCmpGlobalValues(GVL1, GVR2), 1);
}

/// Tests the comparison of non-constant global variables using cmpGlobalValues.
TEST_F(DifferentialFunctionComparatorTest, CmpGlobalValuesNonConstGlobalVars) {
    GlobalVariable *GVL1 =
            new GlobalVariable(*ModL,
                               Type::getInt8Ty(CtxL),
                               false,
                               GlobalValue::ExternalLinkage,
                               ConstantInt::get(Type::getInt32Ty(CtxL), 6),
                               "test.0");
    GlobalVariable *GVR1 =
            new GlobalVariable(*ModR,
                               Type::getInt8Ty(CtxR),
                               false,
                               GlobalValue::ExternalLinkage,
                               ConstantInt::get(Type::getInt32Ty(CtxR), 6),
                               "test.1");
    GlobalVariable *GVR2 =
            new GlobalVariable(*ModR,
                               Type::getInt8Ty(CtxR),
                               false,
                               GlobalValue::ExternalLinkage,
                               ConstantInt::get(Type::getInt32Ty(CtxR), 6),
                               "test2.1");

    ASSERT_EQ(DiffComp->testCmpGlobalValues(GVL1, GVR1), 0);
    ASSERT_EQ(DiffComp->testCmpGlobalValues(GVL1, GVR2), 1);
}

/// Tests the comparison of functions using cmpGlobalValues.
TEST_F(DifferentialFunctionComparatorTest, CmpGlobalValuesFunctions) {
    // Create auxilliary functions for the purpose of inlining tests.
    Function *AuxFL = Function::Create(
            FunctionType::get(Type::getVoidTy(CtxL), {}, false),
            GlobalValue::ExternalLinkage,
            "Aux",
            ModL.get());
    Function *AuxFR = Function::Create(
            FunctionType::get(Type::getVoidTy(CtxR), {}, false),
            GlobalValue::ExternalLinkage,
            "Aux",
            ModR.get());
    ASSERT_EQ(testFunctionComparison(AuxFL, AuxFR), 0);
    ASSERT_NE(ModComp->ComparedFuns.find({AuxFL, AuxFR}),
              ModComp->ComparedFuns.end());

    // Test comparison of print functions (they should be always compared as
    // equal).
    AuxFL = Function::Create(
            FunctionType::get(Type::getVoidTy(CtxL), {}, false),
            GlobalValue::ExternalLinkage,
            "printk",
            ModL.get());
    AuxFR = Function::Create(
            FunctionType::get(Type::getVoidTy(CtxR), {}, false),
            GlobalValue::ExternalLinkage,
            "printk",
            ModR.get());
    ASSERT_EQ(testFunctionComparison(AuxFL, AuxFR), 0);
    ASSERT_EQ(ModComp->ComparedFuns.find({AuxFL, AuxFR}),
              ModComp->ComparedFuns.end());
}

/// Test the comparison of constant global variables with missing initializers
/// using cmpGlobalValues (they should be added to the list of missing
/// definitions).
TEST_F(DifferentialFunctionComparatorTest, CmpGlobalValuesMissingDefs) {
    GlobalVariable *GVL1 = new GlobalVariable(*ModL,
                                              Type::getInt8Ty(CtxL),
                                              true,
                                              GlobalValue::ExternalLinkage,
                                              nullptr);
    GVL1->setName("missing");
    GlobalVariable *GVR1 = new GlobalVariable(*ModR,
                                              Type::getInt8Ty(CtxR),
                                              true,
                                              GlobalValue::ExternalLinkage,
                                              nullptr);
    GVR1->setName("missing2");
    ASSERT_EQ(DiffComp->testCmpGlobalValues(GVL1, GVR1), 1);
    ASSERT_EQ(ModComp->MissingDefs.size(), 1);
    ASSERT_EQ(ModComp->MissingDefs[0].first, GVL1);
    ASSERT_EQ(ModComp->MissingDefs[0].second, GVR1);
}

/// Test ignoring of a cast from a union type using cmpBasicBlocks and
/// cmpValues.
TEST_F(DifferentialFunctionComparatorTest, CmpValuesCastFromUnion) {
    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    StructType *UnionL = StructType::create({Type::getInt8Ty(CtxL)});
    UnionL->setName("union.test");
    Constant *ConstL = ConstantStruct::get(
            UnionL, {ConstantInt::get(Type::getInt8Ty(CtxL), 0)});
    Constant *ConstR = ConstantInt::get(Type::getInt8Ty(CtxR), 0);
    Constant *ConstR2 = ConstantInt::get(Type::getInt8Ty(CtxR), 1);
    CastInst *CastL = new BitCastInst(ConstL, Type::getInt8Ty(CtxL), "", BBL);

    ReturnInst::Create(CtxL, CastL, BBL);
    ReturnInst::Create(CtxR, ConstR, BBR);

    // First, cmpBasicBlocks must be run to identify instructions to ignore
    // and then, cmpValues should ignore those instructions.
    DiffComp->testCmpBasicBlocks(BBL, BBR);
    ASSERT_EQ(DiffComp->testCmpValues(CastL, ConstR), 0);

    BBR->getTerminator()->eraseFromParent();
    ReturnInst::Create(CtxR, ConstR2, BBR);

    DiffComp->testCmpBasicBlocks(BBL, BBR);
    ASSERT_EQ(DiffComp->testCmpValues(CastL, ConstR2), 1);
}

/// Test ignoring of a truncated integer using cmpBasicBlocks and cmpValues.
TEST_F(DifferentialFunctionComparatorTest, CmpValuesIntTrunc) {
    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    Constant *ConstL = ConstantInt::get(Type::getInt16Ty(CtxL), 0);
    Constant *ConstR = ConstantInt::get(Type::getInt16Ty(CtxR), 0);
    CastInst *CastL = new TruncInst(ConstL, Type::getInt8Ty(CtxL), "", BBL);

    ReturnInst::Create(CtxL, CastL, BBL);
    ReturnInst::Create(CtxR, ConstR, BBR);

    // First, cmpBasicBlocks must be run to identify instructions to ignore
    // and then, cmpValues should ignore those instructions.
    DiffComp->testCmpBasicBlocks(BBL, BBR);
    ASSERT_EQ(DiffComp->testCmpValues(CastL, ConstR), -1);

    Conf.Patterns.TypeCasts = true;
    DiffComp->testCmpBasicBlocks(BBL, BBR);
    ASSERT_EQ(DiffComp->testCmpValues(CastL, ConstR), 0);
    ASSERT_EQ(DiffComp->testCmpValues(ConstR, CastL), 0);
    Conf.Patterns.TypeCasts = false;
}

/// Test ignoring of an extended integer value with an unextended one
/// first without arithmetic instructions present (the extension should be
/// ignored), then again with them (the extension should not be ignored).
TEST_F(DifferentialFunctionComparatorTest, CmpValuesIntExt) {
    BasicBlock *BBL = BasicBlock::Create(CtxL, "", FL);
    BasicBlock *BBR = BasicBlock::Create(CtxR, "", FR);

    Constant *ConstL = ConstantInt::get(Type::getInt16Ty(CtxL), 0);
    Constant *ConstR = ConstantInt::get(Type::getInt16Ty(CtxR), 0);
    CastInst *CastL = new SExtInst(ConstL, Type::getInt32Ty(CtxL), "", BBL);

    auto *RetL = ReturnInst::Create(CtxL, CastL, BBL);
    auto *RetR = ReturnInst::Create(CtxR, ConstR, BBR);

    // First, cmpBasicBlocks must be run to identify instructions to ignore
    // and then, cmpValues should ignore those instructions.
    DiffComp->testCmpBasicBlocks(BBL, BBR);
    ASSERT_EQ(DiffComp->testCmpValues(CastL, ConstR), 0);

    RetL->eraseFromParent();
    RetR->eraseFromParent();

    CastInst *CastL2 = new SExtInst(ConstL, Type::getInt64Ty(CtxL), "", BBL);
    BinaryOperator *ArithmL = BinaryOperator::Create(
            Instruction::BinaryOps::Add, CastL2, CastL2, "", BBL);
    BinaryOperator *ArithmR = BinaryOperator::Create(
            Instruction::BinaryOps::Add, ConstR, ConstR, "", BBR);
    ReturnInst::Create(CtxL, ArithmL, BBL);
    ReturnInst::Create(CtxR, ArithmR, BBR);

    DiffComp->testCmpBasicBlocks(BBL, BBR);
    ASSERT_EQ(DiffComp->testCmpValues(CastL2, ConstR), -1);
}

/// Tests comparison of constants that were generated from macros.
TEST_F(DifferentialFunctionComparatorTest, CmpValuesMacroConstantMap) {
    // Create two different constants.
    Constant *ConstL = ConstantInt::get(Type::getInt8Ty(CtxR), 0);
    Constant *ConstR = ConstantInt::get(Type::getInt8Ty(CtxR), 1);

    // Compare them without entries in MacroConstantMap.
    ASSERT_EQ(DiffComp->testCmpValues(ConstL, ConstR), 1);
    ASSERT_EQ(DiffComp->testCmpValues(ConstL, ConstR), 1);

    // Compare them with corresponding entries in MacroConstantMap.
    DbgInfo->MacroConstantMap.insert({ConstL, "1"});
    DbgInfo->MacroConstantMap.insert({ConstR, "0"});

    ASSERT_EQ(DiffComp->testCmpValues(ConstL, ConstR), 0);
    ASSERT_EQ(DiffComp->testCmpValues(ConstL, ConstR), 0);

    // Compare them with non equal entries in MacroConstantMap.
    DbgInfo->MacroConstantMap.erase(ConstL);
    DbgInfo->MacroConstantMap.erase(ConstR);
    DbgInfo->MacroConstantMap.insert({ConstL, "42"});
    DbgInfo->MacroConstantMap.insert({ConstR, "93"});

    ASSERT_EQ(DiffComp->testCmpValues(ConstL, ConstR), 1);
    ASSERT_EQ(DiffComp->testCmpValues(ConstL, ConstR), 1);
}

#if LLVM_VERSION_MAJOR <= 17
// This kind of expression is now deprecated in LLVM, for more information see:
// https://discourse.llvm.org/t/rfc-remove-most-constant-expressions/63179/30
/// Tests comparison of constant expressions containing bitcasts.
TEST_F(DifferentialFunctionComparatorTest, CmpConstants) {
    Conf.Patterns.TypeCasts = true;
    Constant *ConstL = ConstantInt::get(Type::getInt8Ty(CtxR), 0);
    Constant *ConstL2 = ConstantInt::get(Type::getInt8Ty(CtxR), 1);
    Constant *ConstR =
            ConstantExpr::getIntegerCast(ConstL, Type::getInt8Ty(CtxR), false);

    ASSERT_EQ(DiffComp->testCmpConstants(ConstL, ConstR), 0);
    ASSERT_EQ(DiffComp->testCmpConstants(ConstR, ConstL), 0);
    ASSERT_EQ(DiffComp->testCmpConstants(ConstL2, ConstR), -1);
    ASSERT_EQ(DiffComp->testCmpConstants(ConstR, ConstL2), 1);
}
#endif // LLVM_VERSION_MAJOR

TEST_F(DifferentialFunctionComparatorTest, GetCSourceIdentifierType) {
    // Prepare the necessary infrastructure and a basic llvm value (constant)
    std::unordered_map<std::string, const DIType *> LocalVariableMap;
    Function *AuxF = Function::Create(
            FunctionType::get(Type::getVoidTy(CtxL), {}, false),
            GlobalValue::ExternalLinkage,
            "Aux",
            ModL.get());
    Constant *Val = ConstantInt::get(Type::getInt16Ty(CtxL), 0);
    DIBuilder Builder(*ModL);
    DIBasicType *BasicType =
            Builder.createBasicType("int16_t", 16, dwarf::DW_ATE_signed);

    // Local variable, test correct type and debuginfo type
    LocalVariableMap["Aux::LocVar"] = BasicType;
    const DIType *ResType =
            getCSourceIdentifierType("LocVar", AuxF, LocalVariableMap);
    ASSERT_EQ(ResType, BasicType);

    // Global variable, test correct type and debuginfo type
    GlobalVariable *GVar = new GlobalVariable(*ModL,
                                              Val->getType(),
                                              true,
                                              GlobalValue::ExternalLinkage,
                                              Val,
                                              Twine("GlobVar"));
    DIGlobalVariableExpression *GVE = Builder.createGlobalVariableExpression(
            nullptr, "GlobVar", "", nullptr, 0, BasicType, false);
    GVar->addDebugInfo(GVE);
    ResType = getCSourceIdentifierType("GlobVar", AuxF, LocalVariableMap);
    ASSERT_EQ(ResType, BasicType);

    // Dereference of a global variable, test correct debuginfo type
    PointerType *PtrType = PointerType::get(Val->getType(), 0);
    GlobalVariable *GVarPtr = new GlobalVariable(*ModL,
                                                 PtrType,
                                                 true,
                                                 GlobalValue::ExternalLinkage,
                                                 Val,
                                                 Twine("GlobVarPtr"));
    DIDerivedType *DIPtrType = Builder.createPointerType(BasicType, 0);
    DIGlobalVariableExpression *GVEPtr = Builder.createGlobalVariableExpression(
            nullptr, "GlobVarPtr", "", nullptr, 0, DIPtrType, false);
    GVarPtr->addDebugInfo(GVEPtr);
    ResType = getCSourceIdentifierType("*GlobVarPtr", AuxF, LocalVariableMap);
    ASSERT_EQ(ResType, BasicType);

    // Reference of a global variable, test correct type
    ResType = getCSourceIdentifierType("&GlobVar", AuxF, LocalVariableMap);
    ASSERT_EQ(ResType, DIPtrType);
}
