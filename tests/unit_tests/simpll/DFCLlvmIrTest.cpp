//===------------- DFCLlvmIrTest.cpp - Unit tests in LLVM IR --------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Lukas Petr, xpetrl06@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests written using LLVM IR for
/// the DifferentialFunctionComparator class, along with fixture used by them.
///
//===----------------------------------------------------------------------===//

#include "DFCLlvmIrTest.h"
#include <gtest/gtest.h>

/// Methods of test fixture for testing DifferentialFunctionComparator
/// for tests written in LLVM IR.

/// Tries to parse LLVM IR from string and prepares DFC for testing,
/// if parsing was unsuccessful returns false and error is printed.
bool DFCLlvmIrTest::prepare(const char *left, const char *right) {
    ModL = stringToModule(left, CtxL);
    if (ModL == nullptr) {
        errs() << "Error while parsing left LLVM IR\n";
        return false;
    }
    ModR = stringToModule(right, CtxR);
    if (ModR == nullptr) {
        errs() << "Error while parsing right LLVM IR\n";
        return false;
    }
    FL = ModL->getFunction("f");
    if (FL == nullptr) {
        errs() << "Error left LLVM IR: missing test function named `f`\n";
        return false;
    }
    FR = ModR->getFunction("f");
    if (FR == nullptr) {
        errs() << "Error right LLVM IR: missing test function named `f`\n";
        return false;
    }
    prepareDFC();
    return true;
}

/// Tries to parse string containing LLVM IR to module,
/// if unsuccessful prints error message and returns nullptr.
std::unique_ptr<Module> DFCLlvmIrTest::stringToModule(const char *llvm,
                                                      LLVMContext &ctx) {
    SMDiagnostic err;

    StringRef strRef(llvm);
    std::unique_ptr<MemoryBuffer> memBuf = MemoryBuffer::getMemBuffer(strRef);

    std::unique_ptr<Module> mod(parseIR(*memBuf, err, ctx));
    if (mod == nullptr) {
        // Parsing was unsuccessful print error message.
        errs() << "Error in LLVM IR "
               << "(" << std::to_string(err.getLineNo()) << ":"
               << std::to_string(err.getColumnNo()) << ")\n";
        err.print("", errs());
    }
    return mod;
}
