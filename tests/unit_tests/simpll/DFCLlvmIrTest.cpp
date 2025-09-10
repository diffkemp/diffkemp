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

/// Tests a comparison of two GEPs of a structure type with indices compared by
/// value.
TEST_F(DFCLlvmIrTest, CmpGepsSimple) {
    auto left = R"(
        %struct.s = type { i8, i16 }
        define void @f() {
            %var = alloca %struct.s
            %gep1 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 0
            %gep2 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 0
            ret void
        }
    )";
    auto right = R"(
        %struct.s = type { i8, i16 }
        define void @f() {
            %var = alloca %struct.s
            ; same
            %gep1 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 0
            ; different
            %gep2 = getelementptr %struct.s, %struct.s* %var, i32 0, i32 1
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    ASSERT_EQ(testCmpGEPs("gep1"), 0);
    ASSERT_EQ(testCmpGEPs("gep2"), 1);
}

/// Tests a comparison of two GEPs of different array types that don't go into
/// its elements (therefore the type difference should be ignored).
TEST_F(DFCLlvmIrTest, CmpGepsArray) {
    auto left = R"(define void @f() {
        %var = alloca [2 x i8]
        %gep1 = getelementptr [2 x i8], [2 x i8]* %var, i32 0
        %gep2 = getelementptr [2 x i8], [2 x i8]* %var, i32 0
        ret void
    })";
    auto right = R"(define void @f() {
        %var = alloca [3 x i16] ; different array type
        %gep1 = getelementptr [3 x i16], [3 x i16]* %var, i32 0 ; same element
        %gep2 = getelementptr [3 x i16], [3 x i16]* %var, i32 1 ; different
        ret void
    })";
    CREATE_FROM_LLVM(left, right);

    ASSERT_EQ(testCmpGEPs("gep1"), 0);
    ASSERT_EQ(testCmpGEPs("gep2"), -1);
}

/// Tests specific comparison of intermediate comparison operations in cases
/// when the signedness differs when ignoring type casts.
TEST_F(DFCLlvmIrTest, CmpOperationsICmp) {
    bool needToCmpOperands;
    auto left = R"(
        @gv = constant i8 6
        define void @f() {
            %1 = load i8, i8* @gv
            %icmp = icmp ugt i8 %1, %1
            ret void
        }
    )";
    auto right = R"(
        @gv = constant i8 6
        define void @f() {
            %1 = load i8, i8* @gv
            %icmp = icmp sgt i8 %1, %1
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    ASSERT_EQ(testCmpOperations("icmp", needToCmpOperands), -1);
    Conf.Patterns.TypeCasts = true;
    ASSERT_EQ(testCmpOperations("icmp", needToCmpOperands), 0);
}

// Tests that an inverse icmp instruction is only considered inverse when
// the types match.
TEST_F(DFCLlvmIrTest, CmpOperationsWithOpDiffTypes) {
    auto left = R"(define void @f() {
        %1 = add i32 2, 2
        %cond = icmp eq i32 %1, %1
        ret void
    })";
    auto right = R"(define void @f() {
        %1 = add i64 2, 2 ; <-- i64 instead of i32
        %cond = icmp ne i64 %1, %1
        ret void
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_NE(testCmpOperationsWithOperands("cond"), 0);
}

/// Tests specific comparison of allocas of a structure type whose layout
/// changed.
TEST_F(DFCLlvmIrTest, CmpOperationsAllocas) {
    bool needToCmpOperands;
    auto left = R"(
        %struct.s = type { i8, i8 }
        define void @f() {
            %var = alloca %struct.s
            ret void
        }
    )";
    auto right = R"(
        %struct.s = type { i8, i8, i8 } ; <-- added field
        define void @f() {
            %var = alloca %struct.s
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(testCmpOperations("var", needToCmpOperands), 0);
}

/// Tests comparing calls with an extra argument.
TEST_F(DFCLlvmIrTest, CmpCallsWithExtraArg) {
    auto left = R"(
        declare i32 @aux(i32, i32) ; called function with extra argument
        define void @f() {
            %call1 = call i32 @aux(i32 5, i32 6) ; additional param is not zero
            %call2 = call i32 @aux(i32 5, i32 0) ; additional param is zero
            ret void
        }
    )";
    auto right = R"(
        declare i32 @aux(i32)
        define void @f() {
            %call1 = call i32 @aux(i32 5)
            %call2 = call i32 @aux(i32 5)
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(testCmpCallsWithExtraArg("call1"), 1);
    ASSERT_EQ(testCmpCallsWithExtraArg("call2"), 0);
}

/// Tests whether calls are properly marked for inlining while comparing
/// basic blocks.
TEST_F(DFCLlvmIrTest, CmpBasicBlocksInliningLeft) {
    auto left = R"(
        declare void @aux(i32) ; auxilary function for inlining
        define void @f(){
            call void @aux(i32 0)
            ret void
        }
    )";
    auto right = R"(
        define void @f(){
            %var = alloca i8
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    testCmpEntryBasicBlocks();
    // aux function in the left module should be marked for inlining
    auto tryInline = ModComp->tryInline;
    ASSERT_EQ(tryInline.first->getCalledFunction()->getName(), "aux");
    ASSERT_EQ(tryInline.second, nullptr);
}

TEST_F(DFCLlvmIrTest, CmpBasicBlocksInliningRight) {
    auto left = R"(
        define void @f(){
            %var = alloca i8
            ret void
        }
    )";
    auto right = R"(
        declare void @aux(i32) ; auxilary function for inlining
        define void @f(){
            call void @aux(i32 0)
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    testCmpEntryBasicBlocks();
    // aux function in the right module should be marked for inlining
    auto tryInline = ModComp->tryInline;
    ASSERT_EQ(tryInline.first, nullptr);
    ASSERT_EQ(tryInline.second->getCalledFunction()->getName(), "aux");
}

TEST_F(DFCLlvmIrTest, CmpBasicBlocksInliningBoth) {
    auto left = R"(
        declare void @aux(i32)
        define void @f(){
            call void @aux(i32 5)
            ret void
        }
    )";
    auto right = R"(
        declare void @aux(i32)
        define void @f(){
            call void @aux(i32 6) ; calling function with a different argument
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    testCmpEntryBasicBlocks();
    // aux function in both modules should be marked for inlining
    auto tryInline = ModComp->tryInline;
    ASSERT_EQ(tryInline.first->getCalledFunction()->getName(), "aux");
    ASSERT_EQ(tryInline.second->getCalledFunction()->getName(), "aux");
}

/// Tests ignoring of instructions that don't cause a semantic difference in
/// cmpBasicBlocks.
/// Note: the functioning of mayIgnore is tested in the test for cmpValues.
TEST_F(DFCLlvmIrTest, CmpBasicBlocksIgnore) {
    auto left = R"(define void @f() {
        %var = alloca i8
        ret void
    })";
    auto right = R"(define void @f() {
        %var1 = alloca i8
        %var2 = alloca i8 ; <-- additional alloca instruction
        ret void
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(testCmpEntryBasicBlocks(), 0);
    // Swapped functions
    CREATE_FROM_LLVM(right, left);
    ASSERT_EQ(testCmpEntryBasicBlocks(), 0);
}

/// Tests ignoring of pointer casts using cmpBasicBlocks and cmpValues.
TEST_F(DFCLlvmIrTest, CmpValuesPointerCastsLeft) {
    auto left = R"(define void @f() {
        %ptr = inttoptr i32 0 to i8*
        %cast = bitcast i8* %ptr to i32*
        ret void
    })";
    auto right = R"(define void @f() {
        %ptr = inttoptr i32 0 to i8*
        ret void
    })";
    CREATE_FROM_LLVM(left, right);

    // First, cmpBasicBlocks must be run to identify instructions to ignore
    // and then, cmpValues should ignore those instructions.
    testCmpEntryBasicBlocks();
    ASSERT_EQ(testCmpInstValues("ptr", "ptr", true), 0);
    ASSERT_EQ(testCmpInstValues("cast", "ptr", true), 0);
}

TEST_F(DFCLlvmIrTest, CmpValuesPointerCastsRight) {
    auto left = R"(define void @f() {
        %ptr = inttoptr i32 0 to i8*
        ret void
    })";
    auto right = R"(define void @f() {
        %ptr = inttoptr i32 0 to i8*
        %cast = bitcast i8* %ptr to i32*
        ret void
    })";
    CREATE_FROM_LLVM(left, right);

    // First, cmpBasicBlocks must be run to identify instructions to ignore
    // and then, cmpValues should ignore those instructions.
    testCmpEntryBasicBlocks();
    ASSERT_EQ(testCmpInstValues("ptr", "ptr", true), 0);
    ASSERT_EQ(testCmpInstValues("ptr", "cast", true), 0);
}

/// Tests comparison of field access operations with the same offset.
TEST_F(DFCLlvmIrTest, CmpFieldAccessSameOffset) {
    auto left = R"(
        %struct.s2 = type { i8, %struct.s }
        %struct.s = type { i8, i8 }
        define void @f() {
            %1 = alloca %struct.s2
            ; getting the second field of struct.s
            %2 = getelementptr inbounds %struct.s2, %struct.s2* %1, i32 0, i32 1
            %3 = getelementptr inbounds %struct.s, %struct.s* %2, i32 0, i32 1
            ret void
        }
    )";
    auto right = R"(
        %struct.s2 = type { i8, %struct.s }
        %struct.s = type { i8, %union.u } ; <-- second field changed to union
        %union.u = type { i8 }
        define void @f() {
            %1 = alloca %struct.s2
            ; getting the second field of struct.s (it is union here)
            %2 = getelementptr inbounds %struct.s2, %struct.s2* %1, i32 0, i32 1
            %3 = getelementptr inbounds %struct.s, %struct.s* %2, i32 0, i32 1
            %4 = bitcast %union.u* %3 to i8* ; casting union back to inner type
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    // Check if the field accesses are compared correctly and the instruction
    // iterators are at the correct place.
    BasicBlock::const_iterator InstL = FL->getEntryBlock().begin();
    InstL++;
    BasicBlock::const_iterator InstR = FR->getEntryBlock().begin();
    InstR++;

    ASSERT_EQ(DiffComp->testCmpFieldAccess(InstL, InstR), 0);
    // The iterators should point to the instructions following the field access
    // operations if they are equal (which is the terminator of the basic
    // block).
    ASSERT_EQ(&*InstL, FL->getEntryBlock().getTerminator());
    ASSERT_EQ(&*InstR, FR->getEntryBlock().getTerminator());
}

/// Tests comparison of field access operations with a different offset.
TEST_F(DFCLlvmIrTest, CmpFieldAccessDifferentOffset) {
    auto left = R"(
        %struct.s = type { i8, i8 }
        define void @f() {
            %alloca = alloca %struct.s
            ; first field is accessed
            %gep = getelementptr %struct.s, %struct.s* %alloca, i32 0, i32 0
            ret void
        }
    )";
    auto right = R"(
        %struct.s = type { i8, %union.u } ; <-- second field changed to union
        %union.u = type { i8 }
        define void @f() {
            %alloca = alloca %struct.s
            ; second field is accessed
            %gep = getelementptr %struct.s, %struct.s* %alloca, i32 0, i32 1
            %cast = bitcast %union.u* %gep to i8*
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    // Check if the field accesses are compared correctly and the instruction
    // iterators are at the correct place.
    BasicBlock::const_iterator InstL = FL->getEntryBlock().begin();
    InstL++;
    BasicBlock::const_iterator InstR = FR->getEntryBlock().begin();
    InstR++;

    ASSERT_EQ(DiffComp->testCmpFieldAccess(InstL, InstR), 1);
    // The iterators should point to the beginning of the field access
    // operations if they are not equal.
    ASSERT_EQ(&*InstL, getInstByName(*FL, "gep"));
    ASSERT_EQ(&*InstR, getInstByName(*FR, "gep"));
}

/// Tests comparison of field access operations where one ends with a bitcast
/// of a different value than the previous instruction.
TEST_F(DFCLlvmIrTest, CmpFieldAccessBrokenChain) {
    auto left = R"(
        %struct.s = type { i8, i8 }
        define void @f() {
            %alloca = alloca %struct.s
            %gep = getelementptr %struct.s, %struct.s* %alloca, i32 0, i32 1
            ret void
        }
    )";
    auto right = R"(
        %struct.s = type { i8, %union.u } ; <-- second field changed to union
        %union.u = type { i8 }
        define void @f() {
            %alloca = alloca %struct.s
            %gep = getelementptr %struct.s, %struct.s* %alloca, i32 0, i32 1
            ; bitcast of the alloca (not of the gep)
            ; used to break the field access operation
            %cast = bitcast %struct.s* %alloca to i8*
            ret void
        }
    )";
    CREATE_FROM_LLVM(left, right);

    // Check if the field accesses are compared correctly and the instruction
    // iterators are at the correct place.
    BasicBlock::const_iterator InstL = FL->getEntryBlock().begin();
    InstL++;
    BasicBlock::const_iterator InstR = FR->getEntryBlock().begin();
    InstR++;

    ASSERT_EQ(DiffComp->testCmpFieldAccess(InstL, InstR), 0);
    // The iterators should point to the end of the field access operations
    // (i.e. to the return instruction in the left function and to the cast
    // in the other one).
    ASSERT_EQ(&*InstL, FL->getEntryBlock().getTerminator());
    ASSERT_EQ(&*InstR, getInstByName(*FR, "cast"));
}

/// Check that skipping a bitcast instruction doesn't break sizes of
/// synchronisation maps.
TEST_F(DFCLlvmIrTest, CmpSkippedBitcast) {
    auto left = R"(define i32 @f() {
        %1 = alloca i32
        %2 = bitcast i32* %1 to i8*
        ret i32 0
    })";
    auto right = R"(define i32 @f() {
        ret i32 0
    })";
    CREATE_FROM_LLVM(left, right);

    ASSERT_EQ(testCmpEntryBasicBlocks(), 0);
    ASSERT_EQ(DiffComp->getLeftSnMapSize(), DiffComp->getRightSnMapSize());
}

TEST_F(DFCLlvmIrTest, CmpPHIs) {
    auto left = R"(define void @f() {
            br i1 true, label %1, label %2
        1:
            br label %3
        2:
            br label %3
        3:
            %phi1 = phi i8 [ 0, %1 ], [ 1, %2 ]
            %phi2 = phi i8 [ 0, %1 ], [ 1, %2 ]
            %phi3 = phi i8 [ 0, %1 ], [ 1, %2 ]
            ret void
    })";
    auto right = R"(define void @f() {
            br i1 true, label %1, label %2
        1:
            br label %3
        2:
            br label %3
        3:
            %phi1 = phi i8 [ 0, %1 ], [ 1, %2 ]  ; same order (eq)
            %phi2 = phi i8 [ 1, %2 ], [ 0, %1 ]  ; different order (eq)
            %phi3 = phi i8 [ 1, %2 ], [ 1, %2 ]  ; not matching (neq)
            ret void
    })";
    CREATE_FROM_LLVM(left, right);

    ASSERT_EQ(testCmpPHIs("phi1"), 0); // same order (eq)
    ASSERT_EQ(testCmpPHIs("phi2"), 0); // different order (eq)
    ASSERT_EQ(testCmpPHIs("phi3"), 1); // not matching (neq)
}

TEST_F(DFCLlvmIrTest, CustomPatternSkippingInstruction) {
    // Test custom pattern matching and skipping of instructions therein.
    // Creating custom pattern
    LLVMContext PatCtx;
    auto pattern = R"(
        define i8 @diffkemp.old.pattern() {
            %1 = sub i8 0, 1
            ret i8 %1
        }
        define i8 @diffkemp.new.pattern() {
            %1 = sub i8 1, 0
            %2 = sdiv i8 %1, %1
            ret i8 %2
        }
    )";
    auto PatMod = stringToModule(pattern, PatCtx);

    auto left = R"(define i8 @f() {
        %1 = sub i8 0, 1          ; matched
        %2 = call i8 @f()         ; skipped
        ret i8 %1
    })";
    auto right = R"(define i8 @f() {
        %1 = sub i8 1, 0          ; matched
        %2 = call i8 @f()         ; skipped
        %3 = sdiv i8 %1, %1       ; matched
        ret i8 %3
    })";
    CREATE_FROM_LLVM(left, right);

    // Create a pattern set with the pattern module and add it to the comparator
    CustomPatternSet PatSet;
    PatSet.addPatternFromModule(std::move(PatMod));
    DiffComp->addCustomPatternSet(&PatSet);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCSmtTest, SmtDistributive) {
    Conf.SmtTimeout = 2500;
    auto left = R"(define i8 @f() {
        %1 = add i8 1, 0
        %2 = add i8 2, 0
        %3 = add i8 %2, 3
        %4 = mul i8 %1, %3
        ret i8 %4
    })";

    auto right = R"(define i8 @f() {
        %1 = add i8 1, 0
        %2 = add i8 2, 0
        %3 = mul i8 %1, %2
        %4 = mul i8 %1, 3
        %5 = add i8 %3, %4
        ret i8 %5
    })";

    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCSmtTest, SmtMulReplace) {
    auto left = R"(define i8 @f() {
        %1 = add i8 2, 0
        %2 = mul i8 %1, 5
        ret i8 %2
    })";

    auto right = R"(define i8 @f() {
        %1 = add i8 2, 0
        %2 = shl i8 %1, 2
        %3 = add i8 %2, %1
        ret i8 %3
    })";

    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCSmtTest, SmtMulReplaceOverflow) {
    // Use nsw (as if the values were signed integers in a C program).
    // The instructions can produce a poison value, i.e. undefined behavior
    auto left = R"(define i8 @f() {
        %1 = add i8 2, 0
        %2 = mul nsw i8 %1, 5
        ret i8 %2
    })";

    auto right = R"(define i8 @f() {
        %1 = add i8 2, 0
        %2 = shl nsw i8 %1, 2
        %3 = add nsw i8 %2, %1
        ret i8 %3
    })";

    CREATE_FROM_LLVM(left, right);
    ASSERT_NE(DiffComp->compare(), 0);
}

TEST_F(DFCSmtTest, ReorderedBinaryOperationCommutativeSmt) {
    // The same as ReorderedBinaryOperationCommutative but with SMT
    Conf.Patterns.ReorderedBinOps = false;
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

TEST_F(DFCSmtTest, ReorderedBinaryOperationNotCommutativeSmt) {
    // The same as ReorderedBinaryOperationNotCommutative but with SMT
    Conf.Patterns.ReorderedBinOps = false;
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

TEST_F(DFCSmtTest, ReorderedBinaryOperationDifferentOperandsSmt) {
    // The same as ReorderedBinaryOperationDifferentOperands but with SMT
    Conf.Patterns.ReorderedBinOps = false;
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

TEST_F(DFCSmtTest, SmtReorderedThreeInst) {
    Conf.Patterns.ReorderedBinOps = false;
    auto left = R"(define i8 @f() {
        %1 = add i8 0, 1
        %2 = add i8 2, 3
        %3 = add i8 %1, %2
        ret i8 %3
    })";
    auto right = R"(define i8 @f() {
        %1 = add i8 0, 1
        %2 = add i8 %2, 2
        %3 = add i8 %2, 3
        ret i8 %3
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCSmtTest, SmtFloatingPointSimple) {
    // Floating point SMT-solving suffers from exponential blow up, keep it
    // simple -- just do a redundant addition of 0.0
    // Increase the timeout just to make sure that it has enough time
    Conf.SmtTimeout = 5000;
    auto left = R"(define float @f() {
        %1 = fadd float 2.0, 0.0
        %2 = fadd float %1, 0.0
        %3 = fadd float %2, 2.0
        ret float %3
    })";
    auto right = R"(define float @f() {
        %1 = fadd float 2.0, 0.0
        %2 = fadd float %1, 2.0
        ret float %2
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}

TEST_F(DFCSmtTest, SmtInverseCond) {
    auto left = R"(define i1 @f() {
        %1 = add i8 2, 0
        %2 = icmp slt i8 %1, 101
        br i1 %2, label %T, label %F

        T:
            ret i1 1
        F:
            ret i1 0
    })";
    auto right = R"(define i1 @f() {
        %1 = add i8 2, 0
        %2 = icmp sgt i8 %1, 100
        br i1 %2, label %F, label %T

        T:
            ret i1 1
        F:
            ret i1 0
    })";
    CREATE_FROM_LLVM(left, right);
    ASSERT_EQ(DiffComp->compare(), 0);
}
