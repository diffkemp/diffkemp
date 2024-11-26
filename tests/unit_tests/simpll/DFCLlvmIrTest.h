//===-------------- DFCLlvmIrTest.h - Unit tests in LLVM IR ---------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Lukas Petr, xpetrl06@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains declaration of fixture used by unit tests written using
/// LLVM IR for the DifferentialFunctionComparator class.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_DFC_LLVM_IR_TEST_H
#define DIFFKEMP_DFC_LLVM_IR_TEST_H

#include "DifferentialFunctionComparatorTest.h"

/// This macro is supposed to be called from tests which use DFCLlvmIrTest
/// fixture. It tries to parse LLVM IR from string and prepares DFC for testing,
/// if the parsing fails then it fails the test and error message is printed.
/// Note: This is macro so it can FAIL (return from the test case).
#define CREATE_FROM_LLVM(left, right)                                          \
    do {                                                                       \
        if (!prepare(left, right)) {                                           \
            FAIL();                                                            \
        }                                                                      \
    } while (false)

/// Test fixture for testing DifferentialFunctionComparator
/// for tests written in LLVM IR.
class DFCLlvmIrTest : public DifferentialFunctionComparatorTest {
  public:
    DFCLlvmIrTest();

    /// Tries to parse LLVM IR from string and prepares DFC for testing,
    /// if parsing was unsuccessful returns false and error is printed.
    bool prepare(const char *left, const char *right);
    /// Tries to parse string containing LLVM IR to module,
    /// if unsuccessful prints error message and returns nullptr.
    std::unique_ptr<Module> stringToModule(const char *llvm, LLVMContext &ctx);
    /// Returns instruction with the given name from the function
    /// or aborts program if the instruction with the name does not exist.
    Instruction *getInstByName(Function &fun, const char *name);

    /// Methods for testing DifferentialFunctionComparator using the
    /// TestComparator class. The methods cannot be called before
    /// CREATE_FROM_LLVM macro!!!

    /// Compares GEP instructions specified by name using cmpGEPs method.
    int testCmpGEPs(const char *name);
    /// Compares instructions specified by name using cmpOperations method.
    int testCmpOperations(const char *name,
                          bool &needToCmpOperands,
                          bool keepSN = false);
    /// Compares instructions specified by name using cmpOperationsWithOperands
    /// method.
    int testCmpOperationsWithOperands(const char *name, bool keepSN = false);
    /// Compares instructions specified by name using cmpCallsWithExtraArg
    /// method.
    int testCmpCallsWithExtraArg(const char *name, bool keepSN = false);
    /// Compares entry basic blocks in functions using cmpBasicBlocks.
    int testCmpEntryBasicBlocks(bool keepSN = false);
    /// Compares instructions specified by names using cmpValues method.
    int testCmpInstValues(const char *nameL,
                          const char *nameR,
                          bool keepSN = false);
    /// Compares instructions specified by name using cmpPHIs method.
    int testCmpPHIs(const char *name, bool keepSN = false);

  private:
    /// If the fixture is not prepared for testing it prints error and aborts.
    void checkPrepared();
    /// If a value is nullptr it prints error message and aborts the program.
    void checkValue(const char *type, const char *name, Value *L, Value *R);
};

class DFCSmtTest : public DFCLlvmIrTest {
  public:
    DFCSmtTest() : ::DFCLlvmIrTest() { Conf.Patterns.SequentialAluOps = true; }
};

#endif // DIFFKEMP_DFC_LLVM_IR_TEST_H
