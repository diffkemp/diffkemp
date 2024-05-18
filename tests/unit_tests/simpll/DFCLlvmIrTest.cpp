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
#include <llvm/IR/ValueSymbolTable.h>

/// Methods of test fixture for testing DifferentialFunctionComparator
/// for tests written in LLVM IR.

DFCLlvmIrTest::DFCLlvmIrTest() {
    // Setting variables to nullptr, so we can later check if the LLVM IR
    // was parsed.
    ModL = nullptr;
    ModR = nullptr;
    FL = nullptr;
    FR = nullptr;
}

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

/// Returns instruction with the given name from the function
/// or aborts program if the instruction with the name does not exist.
Instruction *DFCLlvmIrTest::getInstByName(Function &fun, const char *name) {
    auto value = fun.getValueSymbolTable()->lookup(name);
    if (value) {
        if (auto inst = dyn_cast<Instruction>(value)) {
            return inst;
        }
    }
    errs() << "Error: Instruction '" << std::string(name)
           << "' does not exist in '" << fun.getName().str() << "' function!\n";
    std::abort();
}

/// Methods for testing DifferentialFunctionComparator using the TestComparator
/// class. The methods cannot be called before CREATE_FROM_LLVM macro!!!

/// Compares GEP instructions specified by name using cmpGEPs method.
int DFCLlvmIrTest::testCmpGEPs(const char *name) {
    checkPrepared();
    auto gepL = dyn_cast<GEPOperator>(getInstByName(*FL, name));
    auto gepR = dyn_cast<GEPOperator>(getInstByName(*FR, name));
    checkValue("GEP instruction", name, gepL, gepR);
    return DiffComp->testCmpGEPs(gepL, gepR);
}

/// Compares instructions specified by name using cmpOperations method.
int DFCLlvmIrTest::testCmpOperations(const char *name,
                                     bool &needToCmpOperands,
                                     bool keepSN) {
    checkPrepared();
    auto instL = getInstByName(*FL, name);
    auto instR = getInstByName(*FR, name);
    return DiffComp->testCmpOperations(instL, instR, needToCmpOperands, keepSN);
}

/// Compares instructions specified by name using cmpOperationsWithOperands
/// method.
int DFCLlvmIrTest::testCmpOperationsWithOperands(const char *name,
                                                 bool keepSN) {
    checkPrepared();
    auto instL = getInstByName(*FL, name);
    auto instR = getInstByName(*FR, name);
    return DiffComp->testCmpOperationsWithOperands(instL, instR, keepSN);
}

/// Compares instructions specified by name using cmpCallsWithExtraArg method.
int DFCLlvmIrTest::testCmpCallsWithExtraArg(const char *name, bool keepSN) {
    checkPrepared();
    auto instL = dyn_cast<CallInst>(getInstByName(*FL, name));
    auto instR = dyn_cast<CallInst>(getInstByName(*FR, name));
    checkValue("CALL instruction", name, instL, instR);
    return DiffComp->testCmpCallsWithExtraArg(instL, instR, keepSN);
}

/// Compares entry basic blocks in functions using cmpBasicBlocks.
int DFCLlvmIrTest::testCmpEntryBasicBlocks(bool keepSN) {
    checkPrepared();
    auto instL = &FL->getEntryBlock();
    auto instR = &FR->getEntryBlock();
    return DiffComp->testCmpBasicBlocks(instL, instR, keepSN);
}

/// Compares instructions specified by names using cmpValues method.
int DFCLlvmIrTest::testCmpInstValues(const char *nameL,
                                     const char *nameR,
                                     bool keepSN) {
    checkPrepared();
    auto valueL = getInstByName(*FL, nameL);
    auto valueR = getInstByName(*FR, nameR);
    return DiffComp->testCmpValues(valueL, valueR, keepSN);
}

/// Compares instructions specified by name using cmpPHIs method.
int DFCLlvmIrTest::testCmpPHIs(const char *name, bool keepSN) {
    checkPrepared();
    auto phiL = dyn_cast<PHINode>(getInstByName(*FL, name));
    auto phiR = dyn_cast<PHINode>(getInstByName(*FR, name));
    checkValue("PHI instruction", name, phiL, phiR);
    return DiffComp->testCmpPHIs(phiL, phiR);
}

/// If the fixture is not prepared for testing it prints error and aborts
/// the program.
void DFCLlvmIrTest::checkPrepared() {
    if (FL == nullptr || FR == nullptr) {
        errs() << "Error: you forgot to call CREATE_FROM_LLVM!\n";
        std::abort();
    }
}

/// If a value is nullptr it prints error message and aborts the program.
void DFCLlvmIrTest::checkValue(const char *type,
                               const char *name,
                               Value *L,
                               Value *R) {
    if (!L || !R) {
        errs() << "Error: '" << name << "' is not " << type << " in "
               << (!L ? "left" : "right") << " function";
        std::abort();
    }
}

/// Unit tests

/// Check that branches with swapped operands and inverse condition are compared
/// as equal.
TEST_F(DFCLlvmIrTest, CmpInverseBranches) {
    auto left = R"(define i1 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %T, label %F
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    auto right = R"(define i1 @f() {
            %1 = icmp ne i1 true, false      ; inverse condition
            br i1 %1, label %F, label %T     ; swapped branches
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

/// Check that branches with swapped operands and conditions such that one is a
/// negation of the other are compared as equal.
TEST_F(DFCLlvmIrTest, CmpInverseBranchesNegation) {
    auto left = R"(define i1 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %T, label %F
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    auto right = R"(define i1 @f() {
            %1 = icmp eq i1 true, false      ; same condition
            %2 = xor i1 %1, true             ; + using not
            br i1 %2, label %F, label %T     ; + swapped branches
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

/// Check that branching with one version of a function containing
/// an inverse condition followed by negation is compared as equal.
TEST_F(DFCLlvmIrTest, CmpInverseBranchesNegation2) {
    auto left = R"(define i1 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %T, label %F
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    auto right = R"(define i1 @f() {
            %1 = icmp ne i1 true, false     ; inverse condition (eq -> ne)
            %2 = xor i1 %1, true            ; + using not
            br i1 %2, label %T, label %F    ; same branching
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

// Check that the combined condition (which for individual conditions
// looks like an inverse condition) is not compared as equal, because it is
// not an inverse condition when the individual conditions are combined.
TEST_F(DFCLlvmIrTest, CombinedCondNotInverseBranches) {
    auto left = R"(define i1 @f() {
            %cond1 = icmp eq i8 5, 5
            %cond2 = icmp eq i8 10, 10
            %or = or i1 %cond1, %cond2           ; 5 == 5 || 10 == 10
            br i1 %or, label %T, label %F        ; same branching
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    auto right = R"(define i1 @f() {
            %cond1 = icmp ne i8 5, 5
            %cond2 = icmp ne i8 10, 10
            %or = or i1 %cond1, %cond2            ; 5 != 5 || 10 != 10
            br i1 %or, label %T, label %F         ; same branching
        T:
            ret i1 true
        F:
            ret i1 false
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_NE(DiffComp->compare(), 0);
}

/// Check detection of code relocation.
TEST_F(DFCLlvmIrTest, CodeRelocation) {
    auto left = R"(
        %struct.s = type { i32, i32 }
        define i32 @f() {
            %var = alloca %struct.s
            %gep1 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 0
            %load1 = load i32, i32* %gep1
            %icmp = icmp ne i32 %load1, 0
            br i1 %icmp, label %1, label %2
        1:
            ; instructions which will be relocated
            %gep2 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 1
            %load2 = load i32, i32* %gep2
            ret i32 %load2
        2:
            ret i32 0
        }
    )";
    auto right = R"(
        %struct.s = type { i32, i32 }
        define i32 @f() {
            %var = alloca %struct.s
            %gep1 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 0
            %load1 = load i32, i32* %gep1

            ; the following two instructions were safely relocated
            %gep2 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 1
            %load2 = load i32, i32* %gep2

            %icmp = icmp ne i32 %load1, 0
            br i1 %icmp, label %1, label %2
        1:
            ret i32 %load2
        2:
            ret i32 0
        }
    )";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

/// Check detection of code relocation when the relocated code is depending on
/// the skipped code. In such a case, the relocation shouldn't be compared as
/// semantics-preserving.
TEST_F(DFCLlvmIrTest, CodeRelocationDependency) {
    auto left = R"(
        %struct.s = type { i32, i32 }
        define i32 @f() {
            %var = alloca %struct.s
            %gep1 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 0
            %load1 = load i32, i32* %gep1
            %gep2 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 1
            store i32 0, i32* %gep2
            %icmp = icmp ne i32 %load1, 0
            br i1 %icmp, label %1, label %2
        1:
            %load2 = load i32, i32* %gep2 ; <--- load after store
            ret i32 %load2
        2:
            ret i32 0
        }
    )";
    auto right = R"(
        %struct.s = type { i32, i32 }
        define i32 @f() {
            %var = alloca %struct.s
            %gep1 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 0
            %load1 = load i32, i32* %gep1
            %gep2 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 1
            %load2 = load i32, i32* %gep2 ; <--- relocated before store
            store i32 0, i32* %gep2
            %icmp = icmp ne i32 %load1, 0
            br i1 %icmp, label %1, label %2
        1:
            ret i32 %load2
        2:
            ret i32 0
        }
    )";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 1);
}

TEST_F(DFCLlvmIrTest, ReorderedPHIs) {
    auto left = R"(define i8 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %2, label %3
        2:
            br label %4
        3:
            br label %4
        4:
            %phi1 = phi i8 [ 1, %2 ], [ 3, %3 ]
            %phi2 = phi i8 [ 2, %2 ], [ 4, %3 ]
            %5 = sub i8 %phi1, %phi2
            ret i8 %5
    })";
    auto right = R"(define i8 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %2, label %3
        2:
            br label %4
        3:
            br label %4
        4:
            ; different order of PHIs
            %phi2 = phi i8 [ 2, %2 ], [ 4, %3 ]
            %phi1 = phi i8 [ 1, %2 ], [ 3, %3 ]
            %5 = sub i8 %phi1, %phi2
            ret i8 %5
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCLlvmIrTest, PHIChain) {
    auto left = R"(define i8 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %2, label %3
        2:
            %var2 = add i8 0, 1
            br label %7
        3:
            br i1 %1, label %4, label %5
        4:
            br label %6
        5:
            br label %6
        6:
            %phi1 = phi i8 [ 0, %4 ], [ 1, %5]
            br label %7
        7:
            %phi2 = phi i8 [ %var2 , %2 ], [ %phi1, %6]
            ret i8 %phi2
    })";
    auto right = R"(define i8 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %2, label %3
        2:
            %var2 = add i8 0, 1
            br label %7
        3:
            br i1 %1, label %4, label %5
        4:
            br label %6
        5:
            br label %6
        6:
            %phi1 = phi i8 [ 0, %4 ], [ 0, %5]
            br label %7
        7:
            %phi2 = phi i8 [ %var2 , %2 ], [ %phi1, %6]
            ret i8 %phi2
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 1);
}

TEST_F(DFCLlvmIrTest, ReorderedPHIsSanityCheck) {
    auto left = R"(define i8 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %2, label %3
        2:
            br label %4
        3:
            br label %4
        4:
            %phi1 = phi i8 [ 1, %2 ], [ 3, %3 ]
            %phi2 = phi i8 [ 2, %2 ], [ 4, %3 ]
            ; swapped order - check that "PHI1" and "PHI2" are not equal
            %5 = sub i8 %phi2, %phi1
            ret i8 %5
    })";
    auto right = R"(define i8 @f() {
            %1 = icmp eq i1 true, false
            br i1 %1, label %2, label %3
        2:
            br label %4
        3:
            br label %4
        4:
            ; different order of PHIs
            %phi2 = phi i8 [ 2, %2 ], [ 4, %3 ]
            %phi1 = phi i8 [ 1, %2 ], [ 3, %3 ]
            %5 = sub i8 %phi1, %phi2
            ret i8 %5
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 1);
}

TEST_F(DFCLlvmIrTest, ReorderedBinaryOperationCommutative) {
    auto left = R"(define i8 @f() {
        %1 = add i8 0, 1
        ret i8 %1
    })";
    auto right = R"(define i8 @f() {
        %1 = add i8 1, 0
        ret i8 %1
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCLlvmIrTest, ReorderedBinaryOperationNotCommutative) {
    auto left = R"(define i8 @f() {
        %1 = sub i8 0, 1
        ret i8 %1
    })";
    auto right = R"(define i8 @f() {
        %1 = sub i8 1, 0
        ret i8 %1
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 1);
}

TEST_F(DFCLlvmIrTest, ReorderedBinaryOperationDifferentOperands) {
    auto left = R"(define i8 @f() {
        %1 = add i8 0, 0
        ret i8 %1
    })";
    auto right = R"(define i8 @f() {
        %1 = add i8 1, 0
        ret i8 %1
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 1);
}

TEST_F(DFCLlvmIrTest, ReorderedBinaryOperationComplex) {
    auto left = R"(define i8 @f() {
        %var = alloca i8
        %load = load i8, i8* %var
        %1 = add i8 1, 2 ; This operation should be skipped,
        %2 = add i8 %1, %load ; operands should be collected and matched here
        ret i8 %2
    })";
    auto right = R"(define i8 @f() {
        %var = alloca i8
        %load = load i8, i8* %var
        %1 = add i8 1, %load
        %2 = add i8 %1, 2
        ret i8 %2
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCLlvmIrTest, ReorderedBinaryOperationNeedLeaf) {
    auto left = R"(define i8 @f() {
        %1 = add i8 1, 2 ; Equal operations, should not be skipped
        %2 = add i8 1, 2 ; Only on one side - should be skipped
        ; Equal, but they do not use the synchronized operands,
        ; we must check the leafs
        %3 = add i8 %2, 1
        ret i8 %3
    })";
    auto right = R"(define i8 @f() {
        %1 = add i8 1, 2
        %2 = add i8 %1, 1
        ret i8 %2
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCLlvmIrTest, SkipRepetitiveLoad) {
    auto left = R"(define i32 @f() {
            %1 = alloca i32
            %2 = load i32, i32* %1
            %3 = icmp ne i32 %2, 0
            br i1 %3, label %4, label %5
        4:
            br label %5
        5:
            ret i32 %2
    })";
    auto right = R"(define i32 @f() {
            %1 = alloca i32
            %2 = load i32, i32* %1
            %3 = icmp ne i32 %2, 0
            br i1 %3, label %4, label %5
        4:
            br label %5
        5:
            %6 = load i32, i32* %1 ; <-- repeating load
            ret i32 %6
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCLlvmIrTest, ReorganizedLocalVariables) {
    auto left = R"(define i8 @f() {
        %1 = add i8 1, 2
        %2 = add i8 %1, %1
        ret i8 %2
    })";

    auto right = R"(
        %struct = type { i8, i8 }
        define i8 @f() {
            %1 = alloca %struct
            %2 = add i8 1, 2
            %3 = getelementptr inbounds %struct, %struct* %1, i32 0, i32 0
            %4 = getelementptr inbounds %struct, %struct* %1, i32 0, i32 1
            store i8 %2, i8* %3
            store i8 %2, i8* %4
            %5 = load i8, i8* %3
            %6 = load i8, i8* %4
            %7 = add i8 %5, %6
            ret i8 %7
        }
    )";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}
