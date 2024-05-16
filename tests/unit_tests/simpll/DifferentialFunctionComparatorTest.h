//===--------- DifferentialFunctionComparatorTest.h - Unit tests -----------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains declaration of classes and fixtures used by unit tests
/// for the DifferentialFunctionComparator class.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_DFC_TEST_H
#define DIFFKEMP_DFC_TEST_H

#include <Config.h>
#include <CustomPatternSet.h>
#include <DebugInfo.h>
#include <DifferentialFunctionComparator.h>
#include <ModuleComparator.h>
#include <ResultsCache.h>
#include <gtest/gtest.h>
#include <memory>
#include <passes/StructureDebugInfoAnalysis.h>
#include <passes/StructureSizeAnalysis.h>

/// This class is used to expose protected methods in
/// DifferentialFunctionComparator.
class TestComparator : public DifferentialFunctionComparator {
  public:
    TestComparator(const Function *F1,
                   const Function *F2,
                   const Config &config,
                   const DebugInfo *DI,
                   const CustomPatternSet *PS,
                   ModuleComparator *MC)
            : DifferentialFunctionComparator(F1, F2, config, DI, PS, MC) {}
    int testCompareSignature(bool keepSN = false);
    int testCmpAttrs(const AttributeList L,
                     const AttributeList R,
                     bool keepSN = false);
    int testCmpAllocs(const CallInst *CL,
                      const CallInst *CR,
                      bool keepSN = false);
    int testCmpConstants(const Constant *CL,
                         const Constant *CR,
                         bool keepSN = false);
    int testCmpMemset(const CallInst *CL,
                      const CallInst *CR,
                      bool keepSN = false);
    int testCmpCallsWithExtraArg(const CallInst *CL,
                                 const CallInst *CR,
                                 bool keepSN = false);
    int testCmpBasicBlocks(BasicBlock *BBL,
                           BasicBlock *BBR,
                           bool keepSN = false);
    int testCmpGEPs(const GEPOperator *GEPL,
                    const GEPOperator *GEPR,
                    bool keepSN = false);
    int testCmpGlobalValues(GlobalValue *L,
                            GlobalValue *R,
                            bool keepSN = false);
    int testCmpValues(const Value *L, const Value *R, bool keepSN = false);
    int testCmpOperations(const Instruction *L,
                          const Instruction *R,
                          bool &needToCmpOperands,
                          bool keepSN = false);
    int testCmpOperationsWithOperands(const Instruction *L,
                                      const Instruction *R,
                                      bool keepSN = false);
    int testCmpTypes(Type *TyL, Type *TyR, bool keepSN = false);
    int testCmpFieldAccess(BasicBlock::const_iterator &InstL,
                           BasicBlock::const_iterator &InstR,
                           bool keepSN = false);
    int testCmpPHIs(PHINode *PhiL, PHINode *PhiR, bool keepSN = false);
    void setLeftSerialNumber(const Value *Val, int i);
    void setRightSerialNumber(const Value *Val, int i);
    size_t getLeftSnMapSize();
    size_t getRightSnMapSize();
    /// Extend the set of custom patterns.
    void addCustomPatternSet(const CustomPatternSet *PatternSet);
};

/// Test fixture providing contexts, modules, functions, a Config object,
/// a ModuleComparator, a TestComparator and debug metadata for the unit tests.
class DifferentialFunctionComparatorTest : public ::testing::Test {
  public:
    // Modules used for testing.
    LLVMContext CtxL, CtxR;
    std::unique_ptr<Module> ModL = std::make_unique<Module>("left", CtxL);
    std::unique_ptr<Module> ModR = std::make_unique<Module>("right", CtxR);

    // Functions to be tested.
    Function *FL, *FR;

    // Objects necessary to create a DifferentialFunctionComparator.
    Config Conf{"F", "F", "", ""};
    std::set<const Function *> CalledFirst;
    std::set<const Function *> CalledSecond;
    ResultsCache Cache{""};
    StructureSizeAnalysis::Result StructSizeMapL;
    StructureSizeAnalysis::Result StructSizeMapR;
    StructureDebugInfoAnalysis::Result StructDIMapL;
    StructureDebugInfoAnalysis::Result StructDIMapR;
    std::unique_ptr<DebugInfo> DbgInfo;
    std::unique_ptr<ModuleComparator> ModComp;

    // TestComparator is used to expose otherwise protected functions.
    std::unique_ptr<TestComparator> DiffComp;

    // Debug metadata is used mainly for checking the detection of macros
    // and types.
    DISubprogram *DSubL, *DSubR;

    /// Initialises functions to be tested (FL, FR)
    /// and prepares DifferentialFunctionComparator.
    DifferentialFunctionComparatorTest();

    /// Prepares DifferentialFunctionComparator.
    void prepareDFC();
    /// Generates a file, compile unit and subprogram for each module.
    void generateDebugMetadata(DICompositeTypeArray DTyArrL = {},
                               DICompositeTypeArray DTyArrR = {},
                               DIMacroNodeArray DMacArrL = {},
                               DIMacroNodeArray DMacArrR = {});
    /// Compares two functions using cmpGlobalValues.
    int testFunctionComparison(Function *FunL, Function *FunR);
};

#endif // DIFFKEMP_DFC_TEST_H
