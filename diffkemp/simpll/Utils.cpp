//===------------------- Utils.cpp - Utility functions --------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementations of utility functions.
///
//===----------------------------------------------------------------------===//

#include "Utils.h"
#include "Config.h"
#include "CustomPatternSet.h"
#include "DebugInfo.h"
#include <algorithm>
#include <iostream>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/LineIterator.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Scalar/DCE.h>
#include <llvm/Transforms/Scalar/NewGVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <numeric>
#include <set>
#include <sstream>

/// Level of debug indentation. Each level corresponds to two characters.
static unsigned int debugIndentLevel = 0;

// Invalid attributes for void functions and calls.
static std::vector<Attribute::AttrKind> badVoidAttributes = {
        Attribute::AttrKind::ByVal,
        Attribute::AttrKind::InAlloca,
        Attribute::AttrKind::Nest,
        Attribute::AttrKind::NoAlias,
        Attribute::AttrKind::NoCapture,
        Attribute::AttrKind::NonNull,
        Attribute::AttrKind::ReadNone,
        Attribute::AttrKind::ReadOnly,
        Attribute::AttrKind::SExt,
        Attribute::AttrKind::StructRet,
        Attribute::AttrKind::ZExt,
        Attribute::AttrKind::Dereferenceable,
        Attribute::AttrKind::DereferenceableOrNull};

std::string programName(Program p) {
    return p == Program::First ? "first" : "second";
}

/// Convert a value to a function.
/// Handles situation when the actual function is inside a bitcast or alias.
const Function *valueToFunction(const Value *Value) {
    auto *Fun = dyn_cast<Function>(Value);
    if (!Fun) {
        if (auto BitCast = dyn_cast<BitCastOperator>(Value)) {
            Fun = dyn_cast<Function>(BitCast->getOperand(0));
        } else if (auto Alias = dyn_cast<GlobalAlias>(Value)) {
            Fun = valueToFunction(Alias->getAliasee());
        }
    }
    return Fun;
}

/// Extract called function from a called value.
/// Handles situation when the called value is a bitcast.
const Function *getCalledFunction(const CallInst *Call) {
    if (!Call)
        return nullptr;
    return valueToFunction(Call->getCalledOperand());
}

/// Extract called function from a called value.
/// Handles situation when the called value is a bitcast.
Function *getCalledFunction(CallInst *Call) {
    return const_cast<Function *>(
            getCalledFunction(const_cast<const CallInst *>(Call)));
}

const Value *getCallee(const CallInst *Call) {
    return Call->getCalledOperand();
}

Value *getCallee(CallInst *Call) {
    return const_cast<Value *>(getCallee(const_cast<const CallInst *>(Call)));
}

/// Extracts value from an arbitrary number of casts.
const Value *stripAllCasts(const Value *Val) {
    if (auto Cast = dyn_cast<CastInst>(Val))
        return stripAllCasts(Cast->getOperand(0));
    else if (auto Cast = dyn_cast<BitCastOperator>(Val))
        // Handle bitcast constant expressions.
        return stripAllCasts(Cast->getOperand(0));
    else
        return Val;
}

/// Extracts value from an arbitrary number of casts.
Value *stripAllCasts(Value *Val) {
    return const_cast<Value *>(stripAllCasts(const_cast<const Value *>(Val)));
}

/// Get name of a type so that it can be used as a variable in Z3.
std::string typeName(const Type *Type) {
    std::string result;
    raw_string_ostream rso(result);
    Type->print(rso);
    rso.str();
    // We must do some modifications to the type name so that is is usable as
    // a Z3 variable
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    std::replace(result.begin(), result.end(), '(', '$');
    std::replace(result.begin(), result.end(), ')', '$');
    std::replace(result.begin(), result.end(), ',', '_');
    return result;
}

/// Find alias which points to the given function and delete it.
void deleteAliasToFun(Module &Mod, Function *Fun) {
    std::vector<GlobalAlias *> toRemove;
    for (auto &alias : Mod.aliases()) {
        if (alias.getAliasee() == Fun)
            toRemove.push_back(&alias);
    }
    for (auto &alias : toRemove) {
        alias->replaceAllUsesWith(Fun);
        alias->eraseFromParent();
    }
}

/// Check if the substring behind the last dot ('.') contains only numbers.
bool hasSuffix(std::string Name) {
    size_t dotPos = Name.find_last_of('.');
    return dotPos != std::string::npos
           && (Name.find_last_not_of("0123456789.") < dotPos
               || Name.substr(dotPos) == ".void");
}

/// Remove everything behind the last dot ('.'). Assumes that hasSuffix returned
/// true for the name.
std::string dropSuffix(std::string Name) {
    return Name.substr(0, Name.find_last_of('.'));
}

/// Join directory path with a filename in case the filename does not already
/// contain the directory.
std::string joinPath(StringRef DirName, StringRef FileName) {
    return FileName.startswith(DirName)
                   ? FileName.str()
                   : DirName.str() + sys::path::get_separator().str()
                             + FileName.str();
}

/// Extracts file name and directory name from the DebugInfo
std::string getFileForFun(const Function *Fun) {
    if (auto *SubProgram = Fun->getSubprogram()) {
        if (auto *File = SubProgram->getFile())
            return joinPath(File->getDirectory(), File->getFilename());
    }
    return "";
}

/// Check if function has side effect (has 'store' instruction or calls some
/// other function with side effect).
bool hasSideEffect(const Function &Fun, std::set<const Function *> &Visited) {
    if (Fun.isDeclaration()) {
        return !(Fun.getIntrinsicID() == Intrinsic::dbg_declare
                 || Fun.getIntrinsicID() == Intrinsic::dbg_value
                 || Fun.getIntrinsicID() == Intrinsic::expect);
    }
    Visited.insert(&Fun);
    for (auto &BB : Fun) {
        for (auto &Inst : BB) {
            if (isa<StoreInst>(&Inst))
                return true;
            if (auto Call = dyn_cast<CallInst>(&Inst)) {
                const Function *called = Call->getCalledFunction();
                if (!called)
                    return true;
                if (Visited.find(called) != Visited.end())
                    continue;
                if (hasSideEffect(*called, Visited))
                    return true;
            }
        }
    }
    return false;
}

/// Calls recursive variant of the function with empty set of visited functions.
bool hasSideEffect(const Function &Fun) {
    std::set<const Function *> visited;
    return hasSideEffect(Fun, visited);
}

/// Returns true if the function is one of the supported allocators
bool isAllocFunction(const Function &Fun) {
    return Fun.getName() == "kzalloc" || Fun.getName() == "__kmalloc"
           || Fun.getName() == "kmalloc";
}

/// Retuns true if the given value is a cast (instruction or constant
/// expression)
bool isCast(const Value *Val) {
    if (isa<CastInst>(Val))
        return true;

    if (auto CExpr = dyn_cast<ConstantExpr>(Val))
        return CExpr->isCast();

    return false;
}

/// Returns true if the given value is a GEP instruction with all indices equal
/// to zero.
bool isZeroGEP(const Value *Val) {
    if (isa<GetElementPtrInst>(Val)) {
        auto Inst = dyn_cast<User>(Val);
        for (unsigned i = 1; i < Inst->getNumOperands(); ++i) {
            auto Int = dyn_cast<ConstantInt>(Inst->getOperand(i));
            if (!(Int && Int->getZExtValue() == 0)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

/// Returns true if the given instruction is a boolean negation operation.
/// LLVM implements negation using "xor X true" (the negated value is always
/// the first operand).
bool isLogicalNot(const Instruction *Inst) {
    // Only accept bool (i1) instructions
    auto intType = dyn_cast<IntegerType>(Inst->getType());
    if (!(intType && intType->getBitWidth() == 1))
        return false;

    if (auto *BinOp = dyn_cast<BinaryOperator>(Inst)) {
        if (BinOp->getOpcode() != Instruction::Xor)
            return false;

        if (auto constOp = dyn_cast<Constant>(BinOp->getOperand(1)))
            return constOp->isAllOnesValue();
    }
    return false;
}

/// Returns true if the given instruction is a reorderable binary operation,
/// i.e., it is commutative and associative. Note that IEEE 754 floating-point
/// addition/multiplication is NOT associative.
bool isReorderableBinaryOp(const Instruction *Inst) {
    if (auto *BinOp = dyn_cast<BinaryOperator>(Inst)) {
        static std::set<Instruction::BinaryOps> ReorderableOps = {
                Instruction::Xor,
                Instruction::Add,
                Instruction::And,
                Instruction::Or,
                Instruction::Mul,
        };
        return (ReorderableOps.count(BinOp->getOpcode()) != 0);
    }
    return false;
}

/// Get value of the given constant as a string
std::string valueAsString(const Constant *Val) {
    if (auto *IntVal = dyn_cast<ConstantInt>(Val)) {
        SmallString<16> StrVal;
        IntVal->getValue().toString(StrVal, 10, true);
        return StrVal.str().str();
    }
    return "";
}

/// Run simplification passes on the function
///  - simplify CFG
///  - dead code elimination
void simplifyFunction(Function *Fun) {
    PassBuilder pb;
    FunctionPassManager fpm;
    FunctionAnalysisManager fam;
    fam.registerPass([] { return AAManager(); });
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(SimplifyCFGPass{});
    fpm.addPass(DCEPass{});
    fpm.addPass(NewGVNPass{});
    fpm.run(*Fun, fam);
}

/// Removes empty attribute sets from an attribute list.
/// This function is used when some attributes are removed to clean up.
AttributeList cleanAttributeList(AttributeList AL, LLVMContext &Context) {
    // Copy over all attributes to a new attribute list.
    AttributeList NewAttrList;

    // There are three possible indices for attribute sets
    std::vector<AttributeList::AttrIndex> indices{AttributeList::FirstArgIndex,
                                                  AttributeList::FunctionIndex,
                                                  AttributeList::ReturnIndex};

    for (AttributeList::AttrIndex i : indices) {
        AttributeSet AttrSet = AL.getAttributes(i);
        if (AttrSet.getNumAttributes() != 0) {
#if LLVM_VERSION_MAJOR < 14
            AttrBuilder AB;
#else
            AttrBuilder AB(Context);
#endif
            for (const Attribute &A : AttrSet)
                AB.addAttribute(A);
#if LLVM_VERSION_MAJOR < 14
            NewAttrList = NewAttrList.addAttributes(Context, i, AB);
#else
            NewAttrList = NewAttrList.addAttributesAtIndex(Context, i, AB);
#endif
        }
    }

    return NewAttrList;
}

/// Get a non-const pointer to a call instruction in a function
CallInst *findCallInst(const CallInst *Call, Function *Fun) {
    if (!Call)
        return nullptr;
    for (auto &BB : *Fun) {
        for (auto &Inst : BB) {
            if (&Inst == Call)
                return dyn_cast<CallInst>(&Inst);
        }
    }
    return nullptr;
}

/// Gets C source file from a DIScope and the module.
std::string getSourceFilePath(DIScope *Scope) {
    return joinPath(Scope->getDirectory(), Scope->getFilename());
}

/// Checks whether the character is valid for a C identifier.
bool isValidCharForIdentifier(char ch) {
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9') || ch == '_')
        return true;
    else
        return false;
}

/// Checks whether the character is valid for the first character of
/// a C identifier.
bool isValidCharForIdentifierStart(char ch) {
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_')
        return true;
    else
        return false;
}

/// Finds the string given in the second argument and replaces it with the one
/// given in the third argument.
void findAndReplace(std::string &input, std::string find, std::string replace) {
    if (find == "")
        return;

    unsigned long position = 0;
    while ((position = input.find(find, position)) != std::string::npos) {
        input.replace(position, find.length(), replace);
        position += replace.length();
    }
}

/// Convert constant expression to instruction. (Copied from LLVM and modified
/// to work outside the ConstantExpr class; otherwise the function is the same,
/// the only purpose of copying the function is making it work on constant
/// input.)
/// Note: this is for constant ConstantExpr pointers; for non-constant ones,
/// the built-in getAsInstruction method is sufficient.
const Instruction *getConstExprAsInstruction(const ConstantExpr *CEx) {
    SmallVector<Value *, 4> ValueOperands(CEx->op_begin(), CEx->op_end());
    ArrayRef<Value *> Ops(ValueOperands);

    switch (CEx->getOpcode()) {
    case Instruction::Trunc:
    case Instruction::ZExt:
    case Instruction::SExt:
    case Instruction::FPTrunc:
    case Instruction::FPExt:
    case Instruction::UIToFP:
    case Instruction::SIToFP:
    case Instruction::FPToUI:
    case Instruction::FPToSI:
    case Instruction::PtrToInt:
    case Instruction::IntToPtr:
    case Instruction::BitCast:
    case Instruction::AddrSpaceCast:
        return CastInst::Create(
                (Instruction::CastOps)CEx->getOpcode(), Ops[0], CEx->getType());
    case Instruction::Select:
        return SelectInst::Create(Ops[0], Ops[1], Ops[2]);
    case Instruction::InsertElement:
        return InsertElementInst::Create(Ops[0], Ops[1], Ops[2]);
    case Instruction::ExtractElement:
        return ExtractElementInst::Create(Ops[0], Ops[1]);
    case Instruction::InsertValue:
        return InsertValueInst::Create(
                Ops[0], Ops[1], dyn_cast<InsertValueInst>(CEx)->getIndices());
    case Instruction::ExtractValue:
        return ExtractValueInst::Create(
                Ops[0], dyn_cast<ExtractValueInst>(CEx)->getIndices());
    case Instruction::ShuffleVector:
        return new ShuffleVectorInst(Ops[0], Ops[1], Ops[2]);

    case Instruction::GetElementPtr: {
        const auto *GO = cast<GEPOperator>(CEx);
        if (GO->isInBounds())
            return GetElementPtrInst::CreateInBounds(
                    GO->getSourceElementType(), Ops[0], Ops.slice(1));
        return GetElementPtrInst::Create(
                GO->getSourceElementType(), Ops[0], Ops.slice(1));
    }
    case Instruction::ICmp:
    case Instruction::FCmp:
        return CmpInst::Create((Instruction::OtherOps)CEx->getOpcode(),
                               (CmpInst::Predicate)CEx->getPredicate(),
                               Ops[0],
                               Ops[1]);

    default:
        assert(CEx->getNumOperands() == 2 && "Must be binary operator?");
        BinaryOperator *BO = BinaryOperator::Create(
                (Instruction::BinaryOps)CEx->getOpcode(), Ops[0], Ops[1]);
        if (isa<OverflowingBinaryOperator>(BO)) {
            BO->setHasNoUnsignedWrap(
                    CEx->getRawSubclassOptionalData()
                    & OverflowingBinaryOperator::NoUnsignedWrap);
            BO->setHasNoSignedWrap(CEx->getRawSubclassOptionalData()
                                   & OverflowingBinaryOperator::NoSignedWrap);
        }
        if (isa<PossiblyExactOperator>(BO))
            BO->setIsExact(CEx->getRawSubclassOptionalData()
                           & PossiblyExactOperator::IsExact);
        return BO;
    }
}

/// Generates human-readable C-like identifier for type.
std::string getIdentifierForType(Type *Ty) {
    if (auto STy = dyn_cast<StructType>(Ty)) {
        // Remove prefix and append "struct"
        if (STy->getStructName().startswith("union"))
            return "union " + STy->getStructName().str().substr(6);
        else if (STy->getStructName().startswith("struct"))
            return "struct " + STy->getStructName().str().substr(7);
        else
            return "<unknown>";
    } else if (auto IntTy = dyn_cast<IntegerType>(Ty)) {
        if (IntTy->getBitWidth() == 1)
            return "bool";
        else
            return "int" + std::to_string(IntTy->getBitWidth()) + "_t";
    } else if (auto ArrTy = dyn_cast<ArrayType>(Ty)) {
        return getIdentifierForType(ArrTy->getElementType()) + "[]";
    } else if (Ty->isVoidTy()) {
        return "void";
    } else if (Ty->isPointerTy()) {
        return "void*";
    } else
        return "<unknown>";
}

/// Generates human-readable C-like identifier for value.
std::string getIdentifierForValue(
        const Value *Val,
        const std::map<std::pair<StructType *, uint64_t>, StringRef>
                &StructFieldNames,
        const Function *Parent) {
    // This function uses a different approach for different types of values.
    if (auto GEPi = dyn_cast<GetElementPtrInst>(Val)) {
        // GEP instruction.
        // First find the original variable name, then try to append the names
        // of all indices.
        std::string name = getIdentifierForValue(
                GEPi->getOperand(0), StructFieldNames, Parent);

        std::vector<Value *> Indices;

        for (auto idx = GEPi->idx_begin(); idx != GEPi->idx_end(); ++idx) {
            auto ValueType = GEPi->getIndexedType(GEPi->getSourceElementType(),
                                                  ArrayRef<Value *>(Indices));
            Value *Index = idx->get();

            if (isa<ConstantInt>(Index)
                && (dyn_cast<ConstantInt>(Index)->getValue().getZExtValue()
                    == 0)
                && (idx == GEPi->idx_begin())) {
                // Do not print the first zero index
                continue;
            }

            if (isa<StructType>(ValueType)) {
                // Structure type indexing
                auto NumericIndex = dyn_cast<ConstantInt>(Index)->getValue();
                auto IndexName =
                        StructFieldNames.find({dyn_cast<StructType>(ValueType),
                                               NumericIndex.getZExtValue()});
                if (IndexName != StructFieldNames.end()) {
                    // We can use the index name to create a C-like syntax.
                    name += "->" + IndexName->second.str();
                } else {
                    name += "->" + std::to_string(NumericIndex.getZExtValue());
                }
            } else {
                // Array type indexing (the index doesn't have to be constant)
                std::string IdxName =
                        getIdentifierForValue(Index, StructFieldNames, Parent);

                // Remove reference operator to match C syntax
                name = name.substr(2, name.size() - 3);

                if (IdxName != "") {
                    name += "[" + IdxName + "]";
                } else {
                    name += "[<unknown>]";
                }
            }

            // We get the pointer to the data, not the data itself.
            name = "&(" + name + ")";
        }

        return name;
    } else if (auto CEx = dyn_cast<ConstantExpr>(Val)) {
        // Constant expressions are converted to instructions.
        return getIdentifierForValue(
                getConstExprAsInstruction(CEx), StructFieldNames, Parent);
    } else if (auto BitCast = dyn_cast<BitCastInst>(Val)) {
        // Bit casts are expanded to C-like cast syntax.
        std::string Casted = getIdentifierForValue(
                BitCast->getOperand(0), StructFieldNames, Parent);
        return "((" + getIdentifierForType(BitCast->getDestTy()) + ") " + Casted
               + ")";
    } else if (auto ZExt = dyn_cast<ZExtInst>(Val)) {
        // ZExt is treated the same as a statement without it
        return getIdentifierForValue(
                ZExt->getOperand(0), StructFieldNames, Parent);
    } else if (auto Load = dyn_cast<LoadInst>(Val)) {
        // Load instruction is treated as the dereference operator
        std::string Internal = getIdentifierForValue(
                Load->getOperand(0), StructFieldNames, Parent);

        if (Internal[0] == '&')
            // Reference and dereference operator cancel out.
            // (delete & and parethenses)
            return Internal.substr(2, Internal.size() - 3);
        else
            return "*(" + Internal + ")";
    } else if (Val->hasName()) {
        // If everything fails, try to get the name directly from the value
        return Val->getName().str();
    } else if (auto Const = dyn_cast<Constant>(Val)) {
        // Constant to string is already implemented in a different function
        return valueAsString(Const);
    } else if (Parent) {
        // Check if the value is a function argument - in case it is, extract
        // the argument name.
        size_t idx = 0;

        // Extract register number
        std::string ValDump;
        llvm::raw_string_ostream ValDumStrm(ValDump);
        Val->print(ValDumStrm);
        std::string RegName = ValDump.substr(ValDump.find_last_of('%') + 1,
                                             std::string::npos);
        size_t RegNum;
        std::istringstream Iss(RegName);
        Iss >> RegNum;
        if (Iss.fail())
            return "<unknown>";

        // Find the argument name
        if (RegNum >= Parent->arg_size())
            // Not a function argument
            return "<unknown>";

        DISubprogram *Sub = Parent->getSubprogram();
        DINodeArray funArgs = Sub->getRetainedNodes();

        for (DINode *Node : funArgs) {
            if (idx == RegNum) {
                DILocalVariable *LocVar = dyn_cast<DILocalVariable>(Node);
                if (!LocVar)
                    return "<unknown>";
                return LocVar->getName().str();
            }
            ++idx;
        }

        return "<unknown>";
    } else
        return "<unknown>";
}

/// Retrieve information about a structured type being pointed to by a value.
/// Note: There are two completely different approaches used. Up to LLVM 14,
/// type information can be obtained directly from the value. Since LLVM 15,
/// type information is obtained from calls to debug intrinsics. It is necessary
/// to provide current function name to use the correct debug intrinsic call
/// (there can be multiple different ones).
TypeInfo getPointeeStructTypeInfo(const Value *Val,
                                  const DataLayout *Layout,
                                  [[maybe_unused]] const StringRef &FunName) {
#if LLVM_VERSION_MAJOR >= 15
    (void)Layout;
    // Look for the type of the value in debug intrinsics
    SmallVector<DbgValueInst *> DbgValues;
    findDbgValues(DbgValues, const_cast<Value *>(Val));

    // There can be potentially multiple different dbg info for the same
    // value. It is necessary to find the one belonging to the current function.
    // The other dbg info can belong to called functions where the value could
    // be provided as (void *) and therefore does not have to contain necessary
    // information about the pointee type.
    DbgValueInst *DbgValue = nullptr;
    for (auto &Dbg : DbgValues) {
        auto scopeName =
                Dbg->getVariable()->getScope()->getSubprogram()->getName();
        if (scopeName == FunName) {
            DbgValue = Dbg;
            break;
        }
    }
    if (!DbgValue)
        return {"", 0};
    const DIType *Ty = DbgValue->getVariable()->getType();

    // Check if it is a pointer type (derived type)
    const DIDerivedType *PtrTy = dyn_cast<DIDerivedType>(Ty);
    if (!PtrTy)
        return {"", 0};

    // Get the pointee type
    const DIType *PointeeTy = PtrTy->getBaseType();
    if (!PointeeTy) {
        // Base type is null -> the pointer type is void *.
        return {"", 0};
    }

    // Check if the pointee type is a structured type (composite type)
    const DICompositeType *StrTy = dyn_cast<DICompositeType>(PointeeTy);
    if (!StrTy)
        return {"", 0};

    return {StrTy->getName(), StrTy->getSizeInBits() / 8};
#else
    // Get the type of the value
    Type *Ty = Val->getType();

    // It may be a pointer type, retrieve the pointee type from it
    PointerType *PtrTy = dyn_cast<PointerType>(Ty);
    Type *PointeeTy = PtrTy ? PtrTy->getPointerElementType() : Ty;

    // Check if the pointee type is a structured type
    StructType *StrTy = dyn_cast<StructType>(PointeeTy);
    if (!StrTy)
        return {"", 0};

    return {StrTy->getName(), Layout->getTypeStoreSize(StrTy)};
#endif
}

/// Retrieves the type of the value based on its C source code expression.
const DIType *getCSourceIdentifierType(std::string expr,
                                       const Function *Parent,
                                       const LocalVariableMap &LVMap) {
    // First we have to remove pointer operators from the call.
    if (expr[0] == '*') {
        // Dereference operator. Return the original type.
        const DIType *DereferencedTy =
                getCSourceIdentifierType(expr.substr(1), Parent, LVMap);
        if (!DereferencedTy || !isa<DIDerivedType>(DereferencedTy))
            return nullptr;
        const DIDerivedType *PointerTy =
                dyn_cast<DIDerivedType>(DereferencedTy);
        return PointerTy->getBaseType();
    }
    if (expr[0] == '&') {
        // Reference operator. Return a pointer type.
        const DIType *ReferencedType =
                getCSourceIdentifierType(expr.substr(1), Parent, LVMap);
        // Note: assuming von Neumann architecture with single address space.
        if (!ReferencedType)
            return nullptr;
        DIBuilder Builder(*(const_cast<Module *>(Parent->getParent())));
        return Builder.createPointerType(const_cast<DIType *>(ReferencedType),
                                         0);
    }
    // Determine whether the expression is an identifier at this point.
    // If not, it is not supported.
    std::vector<bool> Tmp;
    std::transform(expr.begin(),
                   expr.end(),
                   std::back_inserter(Tmp),
                   isValidCharForIdentifier);
    if (!std::accumulate(Tmp.begin(), Tmp.end(), true, [](auto a, auto b) {
            return a && b;
        })) {
        // There are some characters that are not allowed in an identifier.
        return nullptr;
    }

    // Now try to look up the identifier first in global variables, then in
    // local variables.
    auto Glob = Parent->getParent()->getGlobalVariable(expr);
    if (Glob) {
#if LLVM_VERSION_MAJOR >= 12
        SmallVector<DIGlobalVariableExpression *> GVs;
#else
        SmallVector<DIGlobalVariableExpression *, 1> GVs;
#endif
        Glob->getDebugInfo(GVs);
        if (!GVs.empty())
            return GVs[0]->getVariable()->getType();
    }

    auto Loc = LVMap.find(Parent->getName().str() + "::" + expr);
    if (Loc != LVMap.end())
        return Loc->second;

    // If everything failed, return null.
    return nullptr;
}

/// Copies properties from one call instruction to another.
void copyCallInstProperties(CallInst *srcCall, CallInst *destCall) {
    destCall->setAttributes(srcCall->getAttributes());
    destCall->setCallingConv(srcCall->getCallingConv());
    destCall->setDebugLoc(srcCall->getDebugLoc());

    if (srcCall->isTailCall()) {
        destCall->setTailCall();
    }

    if (!srcCall->getType()->isVoidTy() && destCall->getType()->isVoidTy()) {
        // Remove attributes that are incompatible with void calls.
        for (Attribute::AttrKind AK : badVoidAttributes) {
#if LLVM_VERSION_MAJOR < 14
            destCall->removeAttribute(AttributeList::ReturnIndex, AK);
            destCall->removeAttribute(AttributeList::FunctionIndex, AK);
#else
            destCall->removeAttributeAtIndex(AttributeList::ReturnIndex, AK);
            destCall->removeAttributeAtIndex(AttributeList::FunctionIndex, AK);
#endif
        }

        destCall->setAttributes(cleanAttributeList(destCall->getAttributes(),
                                                   destCall->getContext()));
    }
}

/// Copies properties from one function to another.
void copyFunctionProperties(Function *srcFun, Function *destFun) {
    destFun->copyAttributesFrom(srcFun);
    destFun->setSubprogram(srcFun->getSubprogram());

    if (!srcFun->getType()->isVoidTy() && destFun->getType()->isVoidTy()) {
        for (Attribute::AttrKind AK : badVoidAttributes) {
            // Remove attributes that are incompatible with void functions.
#if LLVM_VERSION_MAJOR < 14
            destFun->removeAttribute(AttributeList::ReturnIndex, AK);
            destFun->removeAttribute(AttributeList::FunctionIndex, AK);
#else
            destFun->removeAttributeAtIndex(AttributeList::ReturnIndex, AK);
            destFun->removeAttributeAtIndex(AttributeList::FunctionIndex, AK);
#endif
        }
        destFun->setAttributes(cleanAttributeList(destFun->getAttributes(),
                                                  destFun->getContext()));
    }

    // Set the names of all arguments of the new function
    for (Function::arg_iterator AI = srcFun->arg_begin(),
                                AE = srcFun->arg_end(),
                                NAI = destFun->arg_begin();
         AI != AE;
         ++AI, ++NAI) {
        NAI->takeName(AI);
    }
}

/// Tests whether two names of types or globals match. Names match if they
/// are the same or if the DiffKemp pattern name prefixes are used.
bool namesMatch(const StringRef &L, const StringRef &R, bool IsLeftSide) {
    // Remove number suffixes
    std::string NameL = hasSuffix(L.str()) ? dropSuffix(L.str()) : L.str();
    std::string NameR = hasSuffix(R.str()) ? dropSuffix(R.str()) : R.str();

    // Compare the names themselves.
    if (NameL == NameR)
        return true;

    // If no prefix is present, the names are not equal.
    StringRef NameRRef = NameR;
    if (!NameRRef.startswith(CustomPatternSet::DefaultPrefix))
        return false;

    // Remove all prefixes.
    auto PrefixR =
            IsLeftSide ? CustomPatternSet::PrefixL : CustomPatternSet::PrefixR;
    StringRef RealNameRRef =
            NameRRef.substr(CustomPatternSet::DefaultPrefix.size());

    if (RealNameRRef.startswith(PrefixR))
        RealNameRRef = RealNameRRef.substr(PrefixR.size());

    // Compare the names without prefixes.
    return NameL == RealNameRRef;
}

/// Converts value to its string representation.
/// Note: Currently the only place that calls this is returns.gdb, which lacks
/// the ability to directly dump values because GDB can't call the corresponding
/// methods.
std::string valueToString(const Value *Val) {
    std::string ValDump;
    llvm::raw_string_ostream DumpStrm(ValDump);
    Val->print(DumpStrm);
    return DumpStrm.str();
}

/// Converts type to its (LLVM IR) string representation.
std::string typeToString(Type *Ty) {
    std::string TyDump;
    llvm::raw_string_ostream DumpStrm(TyDump);
    Ty->print(DumpStrm);
    return DumpStrm.str();
}

/// Get a string matching the current indentation level.
/// \param prefix Indentation prefix character, defaults to space.
std::string getDebugIndent(const char prefixChar) {
    return std::string(debugIndentLevel * 2, prefixChar);
}

/// Increase the level of debug indentation by one.
void increaseDebugIndentLevel() { debugIndentLevel++; }

/// Decrease the level of debug indentation by one.
void decreaseDebugIndentLevel() {
    assert(debugIndentLevel > 0);
    debugIndentLevel--;
}

/// Inline a function call and return true if inlining succeeded.
bool inlineCall(CallInst *Call) {
    InlineFunctionInfo ifi;
#if LLVM_VERSION_MAJOR >= 16
    return InlineFunction(*Call, ifi, false, nullptr, false).isSuccess();
#elif LLVM_VERSION_MAJOR >= 11
    return InlineFunction(*Call, ifi, nullptr, false).isSuccess();
#else
    return InlineFunction(Call, ifi, nullptr, false);
#endif
}

namespace Color {
const std::string RED = "\e[1;31m";
const std::string GREEN = "\e[1;32m";
const std::string YELLOW = "\e[0;93m";
const std::string WHITE = "\e[0m";

std::string makeRed(std::string text) {
    return dbgs().has_colors() ? (RED + text + WHITE) : text;
}
std::string makeGreen(std::string text) {
    return dbgs().has_colors() ? (GREEN + text + WHITE) : text;
}
std::string makeYellow(std::string text) {
    return dbgs().has_colors() ? (YELLOW + text + WHITE) : text;
}
} // namespace Color

/// Return LLVM struct type of the given name
StructType *getTypeByName(const Module &Mod, StringRef Name) {
#if LLVM_VERSION_MAJOR >= 12
    return StructType::getTypeByName(Mod.getContext(), Name);
#else
    return Mod.getTypeByName(Name);
#endif
}

/// Given an instruction and a pointer value, try to determine whether the
/// instruction may store to the memory pointed to by the pointer. This can
/// happen only if the instruction is a store or a function call.
bool mayStoreTo(const Instruction *Inst, const Value *Ptr) {
    if (auto Store = dyn_cast<StoreInst>(Inst)) {
        // If the instruction is a store, we check whether its pointer operand
        // may alias the given pointer.
        return mayAlias(Store->getPointerOperand(), Ptr);
    }
    if (auto Call = dyn_cast<CallInst>(Inst)) {
        // If the instruction is a call, we preventively return true
        // unless it is a debug call.
        return !isDebugInfo(*Call);
    }
    return false;
}

/// Given two pointer values, try do determine whether they may alias. The
/// function currently supports only simple aliasing of local memory.
bool mayAlias(const Value *PtrL, const Value *PtrR) {
    auto AllocaL = getAllocaFromPtr(PtrL);
    auto AllocaR = getAllocaFromPtr(PtrR);
    if (!AllocaL || !AllocaR)
        // If any of the two pointers does not directly point to local memory,
        // do not continue; more advanced alias analysis would be necessary.
        return true;
    if (AllocaL != AllocaR)
        // If the underlying allocas are different, the pointers cannot alias.
        return false;
    auto GEPL = dyn_cast<GetElementPtrInst>(PtrL);
    auto GEPR = dyn_cast<GetElementPtrInst>(PtrR);
    if (!GEPL || !GEPR)
        // At this point, we know that the pointers point to the same alloca.
        // If any of the pointers is the alloca itself, the pointers alias.
        return true;
    auto GEPLIdxIt = GEPL->idx_begin(), GEPLIdxE = GEPL->idx_end();
    auto GEPRIdxIt = GEPR->idx_begin(), GEPRIdxE = GEPR->idx_end();
    // Go through the indices of both GEPs in parallel. If they diverge at some
    // point, the pointers do not alias.
    while (GEPLIdxIt != GEPLIdxE && GEPRIdxIt != GEPRIdxE) {
        auto ConstantIdxL = dyn_cast<ConstantInt>(GEPLIdxIt->get());
        auto ConstantIdxR = dyn_cast<ConstantInt>(GEPRIdxIt->get());
        if (ConstantIdxL && ConstantIdxR
            && ConstantIdxL->getSExtValue() != ConstantIdxR->getSExtValue())
            return false;
        GEPLIdxIt++;
        GEPRIdxIt++;
    }
    return true;
}

/// Given a pointer value, return the instruction which allocated the memory
/// where the pointer points. Return a null pointer if an alloca is not found,
/// e.g. because the pointer is a function parameter.
const AllocaInst *getAllocaFromPtr(const Value *Ptr) {
    if (auto Alloca = dyn_cast<AllocaInst>(Ptr)) {
        return Alloca;
    }
    if (auto GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
        return getAllocaFromPtr(GEP->getPointerOperand());
    }
    return nullptr;
}
