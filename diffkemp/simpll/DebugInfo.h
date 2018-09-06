//===------------- DebugInfo.h - Processing debug information -------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains classes and LLVM passes for processing and analysing
/// debugging information in the compared modules.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_DEBUGINFO_H
#define DIFFKEMP_SIMPLL_DEBUGINFO_H

#include "Utils.h"
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/TypeFinder.h>
#include <set>
#include <llvm/IR/ValueMap.h>

using namespace llvm;

/// Analysing debug info of the module and extracting useful information.
/// The following information is extracted:
/// 1. Alignment of GEP indices.
///    In case the corresponding structure has a different set of fields between
///    the analysed modules, it might happen that corresponding fields are at
///    different indices. This analysis matches fields with same name and saves
///    the index offset into the metadata of a GEP instruction.
class DebugInfo {
  using StructFieldNamesMap = std::map<std::pair<StructType *, uint64_t>,
                                    StringRef>;

  public:
    DebugInfo(Module &modFirst, Module &modSecond,
              Function *funFirst, Function *funSecond,
              std::set<const Function *> &CalledFirst) :
            FunFirst(funFirst), FunSecond(funSecond),
            ModFirst(modFirst), ModSecond(modSecond),
            CalledFirst(CalledFirst) {
        DebugInfoFirst.processModule(ModFirst);
        DebugInfoSecond.processModule(ModSecond);
        calculateGEPIndexAlignments();
    };

    /// Maps structure type and index to struct member names
    StructFieldNamesMap StructFieldNames;

  private:
    Function *FunFirst;
    Function *FunSecond;
    Module &ModFirst;
    Module &ModSecond;
    DebugInfoFinder DebugInfoFirst;
    DebugInfoFinder DebugInfoSecond;
    std::set<const Function *> &CalledFirst;

    /// Mapping struct types to index maps that contain pairs of corresponding
    /// indices.
    std::map<StructType *, std::map<uint64_t, uint64_t>> IndexMaps;

    /// Calculate alignments of the corresponding indices of GEP instructions.
    void calculateGEPIndexAlignments();

    /// Get debug info for struct type with given name
    DICompositeType *getStructTypeInfo(const StringRef name,
                                       const Program prog) const;

    /// Get index of the struct member having the given name
    /// \param type Struct type debug info
    /// \param name Searched name
    static int getTypeMemberIndex(const DICompositeType &type,
                                  const StringRef name);

    /// Get name of the struct member on the given index
    /// \param type Struct type debug info.
    /// \param index Index of struct field.
    /// \return Name of the field.
    static StringRef getElementNameAtIndex(const DICompositeType &type,
                                           uint64_t index);

    /// Add metadata with the new offset to the GEP instruction
    /// \param GEP GEP instruction.
    /// \param index GEP instruction index.
    /// \param alignment New alignment.
    /// \param bitWidth Alignment bit width.
    /// \param c LLVM context.
    static void setNewAlignmentOfIndex(GetElementPtrInst &GEP,
                                       unsigned long index,
                                       uint64_t alignment,
                                       unsigned bitWidth,
                                       LLVMContext &c);

    /// Check if the struct element has the same index as the previous element
    /// (this situation may be caused by the compiler due to struct alignment).
    static bool isSameElemIndex(const DIDerivedType *TypeElem);
};

/// A pass to remove all debugging information from a function.
class RemoveDebugInfoPass : public PassInfoMixin<RemoveDebugInfoPass> {
  public:
    PreservedAnalyses run(Function &Fun,
                          FunctionAnalysisManager &fam);
};

/// Check if function is a debug info function.
bool isDebugInfo(const Function &Fun);
/// Check if instruction is a call to debug info function.
bool isDebugInfo(const Instruction &Instr);

/// Get name of a struct type as it is specified in the C source.
std::string getStructTypeName(const StructType *type);

#endif // DIFFKEMP_SIMPLL_DEBUGINFO_H
