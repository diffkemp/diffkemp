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
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Support/LineIterator.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <set>
#include <iostream>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar/DCE.h>
#include <llvm/Transforms/Scalar/NewGVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/Utils/Cloning.h>

/// Extract called function from a called value Handles situation when the
/// called value is a bitcast.
const Function *getCalledFunction(const Value *CalledValue) {
    const Function *fun = dyn_cast<Function>(CalledValue);
    if (!fun) {
        if (auto BitCast = dyn_cast<BitCastOperator>(CalledValue)) {
            fun = dyn_cast<Function>(BitCast->getOperand(0));
        }
    }
    return fun;
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
            && Name.find_last_not_of("0123456789.") < dotPos;
}

/// Remove everything behind the last dot ('.'). Assumes that hasSuffix returned
/// true for the name.
std::string dropSuffix(std::string Name) {
    return Name.substr(0, Name.find_last_of('.'));
}

/// Extracts file name and directory name from the DebugInfo
std::string getFileForFun(Function *Fun) {
    if (auto *SubProgram = Fun->getSubprogram()) {
        if (auto *File = SubProgram->getFile()) {
            return File->getDirectory().str() + "/" + File->getFilename().str();
        }
    }
    return "";
}

/// Recursive search for call stack from Src to Dest
bool searchCallStackRec(Function *Src,
                        Function *Dest,
                        CallStack &callStack,
                        std::set<Function *> &visited) {
    visited.insert(Src);
    for (auto &BB : *Src) {
        for (auto &Inst : BB) {
            // Collect all functions occuring in the instruction.
            // Function can be either called or used as a parameter
            std::vector<Function *> calledFuns;
            if (CallInst *Call = dyn_cast<CallInst>(&Inst)) {
                if (auto c = Call->getCalledFunction())
                    calledFuns.push_back(c);
            }
            for (auto &Op : Inst.operands()) {
                if (isa<Function>(Op))
                    calledFuns.push_back(dyn_cast<Function>(Op));
            }

            // Follow each found function
            for (Function *called : calledFuns) {
                if (called && visited.find(called) == visited.end()) {
                    auto loc = Inst.getDebugLoc();
                    if (!loc)
                        continue;
                    callStack.push_back(CallInfo(called,
                                                 getFileForFun(Src),
                                                 loc.getLine()));
                    if (called == Dest)
                        return true;
                    else {
                        bool found = searchCallStackRec(called, Dest, callStack,
                                                        visited);
                        if (found)
                            return true;
                        else
                            callStack.pop_back();
                    }
                }
            }
        }
    }
    return false;
}

/// Calls the recursive function that retrieves the call stack
CallStack getCallStack(Function &Src, Function &Dest) {
    CallStack callStack;
    std::set<Function *> visited;
    searchCallStackRec(&Src, &Dest, callStack, visited);
    return callStack;
}

/// Check if function has side effect (has 'store' instruction or calls some
/// other function with side effect).
bool hasSideEffect(const Function &Fun, std::set<const Function *> &Visited) {
    if (Fun.isDeclaration()) {
        return !(Fun.getIntrinsicID() == Intrinsic::dbg_declare ||
                 Fun.getIntrinsicID() == Intrinsic::dbg_value ||
                 Fun.getIntrinsicID() == Intrinsic::expect);
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
    return Fun.getName() == "kzalloc" || Fun.getName() == "__kmalloc" ||
           Fun.getName() == "kmalloc";
}

/// Get value of the given constant as a string
std::string valueAsString(const Constant *Val) {
    if (auto *IntVal = dyn_cast<ConstantInt>(Val)) {
        return std::to_string(IntVal->getSExtValue());
    }
    return "";
}

/// Extract struct type of the value.
/// Works if the value is of pointer type which can be even bitcasted.
/// Returns nullptr if the value is not a struct.
StructType *getStructType(const Value *Value) {
    StructType *Type = nullptr;
    if (auto PtrTy = dyn_cast<PointerType>(Value->getType())) {
        // Value is a pointer
        if (auto *StructTy = dyn_cast<StructType>(PtrTy->getElementType())) {
            // Value points to a struct
            Type = StructTy;
        } else if (auto *BitCast = dyn_cast<BitCastInst>(Value)) {
            // Value is a bicast, get the original type
            if (auto *SrcPtrTy = dyn_cast<PointerType>(BitCast->getSrcTy())) {
                if (auto *SrcStructTy =
                        dyn_cast<StructType>(SrcPtrTy->getElementType()))
                    Type = SrcStructTy;
            }
        }
    } else
        Type = dyn_cast<StructType>(Value->getType());
    return Type;
}

/// Run simplification passes on the function
///  - simplify CFG
///  - dead code elimination
void simplifyFunction(Function *Fun) {
    PassBuilder pb;
    FunctionPassManager fpm(false);
    FunctionAnalysisManager fam(false);
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(SimplifyCFGPass {});
    fpm.addPass(DCEPass {});
    fpm.addPass(NewGVNPass {});
    fpm.run(*Fun, fam);
}

/// Removes empty attribute sets from an attribute list.
/// This function is used when some attributes are removed to clean up.
AttributeList cleanAttributeList(AttributeList AL) {
    // Copy over all attributes to a new attribute list.
    AttributeList NewAttrList;

    // There are three possible indices for attribute sets
    std::vector<AttributeList::AttrIndex> indices {
        AttributeList::FirstArgIndex,
        AttributeList::FunctionIndex,
        AttributeList::ReturnIndex
    };

    for (AttributeList::AttrIndex i : indices) {
        AttributeSet AttrSet =
                AL.getAttributes(i);
        if (AttrSet.getNumAttributes() != 0) {
            AttrBuilder AB;
            for (const Attribute &A : AttrSet)
                AB.addAttribute(A);
            NewAttrList = NewAttrList.addAttributes(
                    AL.getContext(), i, AB);
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
std::string getSourceFilePath(DIScope *Scope, const Module *Mod) {
    // Extract the source file path.
    StringRef parentDirectory = llvm::sys::path::parent_path(
            Mod->getModuleIdentifier());

    while (!llvm::sys::path::filename(parentDirectory).startswith("linux")) {
        parentDirectory = llvm::sys::path::parent_path(parentDirectory);
        if (parentDirectory == "")
            // Avoid infinite loop
            return "";
    }

    std::string sourceFilePath = parentDirectory.str() +
            llvm::sys::path::get_separator().str() +
            Scope->getFilename().str();

    return sourceFilePath;
}

bool isValidCharForIdentifier(char ch) {
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == '_')
        return true;
    else
        return false;
}

/// Gets all macros used on the line in the form of a key to value map.
std::unordered_map<std::string, MacroElement> getAllMacrosOnLine(
    StringRef line, StringMap<MacroElement> macroMap) {
    // Create a second map, but only with macros that are used on the line.
    // Note: For the purpose of the algorithm the starting line is treated as a
    // with a space as its key, making it an invalid identifier for a macro.
    std::unordered_map<std::string, MacroElement> usedMacroMap;
    bool mapChanged = true;
    usedMacroMap[" "] = MacroElement {"<>", line, StringRef(""), 0, nullptr};

    while (mapChanged) {
        mapChanged = false;
        std::vector<std::pair<StringRef, MacroElement>> entriesToAdd;
        for (auto &Entry : usedMacroMap) {
            // Process all substrings of the macro value to look for any other
            // macros that could be contained inside it
            std::string macroBody = Entry.second.body.str();
            for (int i = 0; i < macroBody.length(); i++) {
                for (int j = 1; j < (macroBody.length() - i); j++) {
                    auto potentialMacro =
                            macroMap.find(macroBody.substr(i, j));

                    if (i > 0 && isValidCharForIdentifier(macroBody[i - 1]))
                        continue;
                    if (isValidCharForIdentifier(macroBody[i + j]))
                        continue;

                    if (potentialMacro != macroMap.end()) {
                        int lengthBefore = usedMacroMap.size();
                        MacroElement macro = potentialMacro->second;
                        macro.parentMacro = Entry.first;
                        entriesToAdd.push_back({potentialMacro->first(),
                                                macro});
                    }
                }
            }
        }

        int originalMapSize = usedMacroMap.size();
        for (std::pair<StringRef, MacroElement> &Entry : entriesToAdd) {
            if (usedMacroMap.find(Entry.first.str()) == usedMacroMap.end()) {
                usedMacroMap[Entry.first.str()] = Entry.second;
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << "Adding macro " << Entry.first <<
                                " : " << Entry.second.body << ", parent macro "
                                << Entry.second.parentMacro << "\n");
            }
        }

        if (originalMapSize < usedMacroMap.size())
            mapChanged = true;
    }

    // Remove the line itself from the macro maps (they should contain only
    // macros)
    usedMacroMap.erase(" ");

    return usedMacroMap;
}

/// Gets all macros used on a certain DILocation in the form of a key to value
/// map.
std::unordered_map<std::string, MacroElement> getAllMacrosAtLocation(
    DILocation *LineLoc, const Module *Mod) {
    if (!LineLoc || LineLoc->getNumOperands() == 0) {
        // DILocation has no scope or is not present - cannot get macro stack
        DEBUG_WITH_TYPE(DEBUG_SIMPLL, dbgs() << "Scope for macro not found\n");
        return std::unordered_map<std::string, MacroElement>();
    }

    // Open the file and extract the line
    auto sourceFile = MemoryBuffer::getFile(Twine(getSourceFilePath(
            dyn_cast<DIScope>(LineLoc->getScope()), Mod)));
    if (sourceFile.getError()) {
        // Source file was not found, return empty maps
        DEBUG_WITH_TYPE(DEBUG_SIMPLL, dbgs() << "Source for macro not found\n");
        return std::unordered_map<std::string, MacroElement>();
    }

    line_iterator it(**sourceFile);
    StringRef line;
    while (!it.is_at_end() && it.line_number() != (LineLoc->getLine())) {
        ++it; line = *it;
    }

    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                    dbgs() << "Looking for all macros on line:" << line
                    << "\n");

    // Get macro array from debug info
    DISubprogram *Sub = LineLoc->getScope()->getSubprogram();
    DIMacroNodeArray RawMacros = Sub->getUnit()->getMacros();

    // Create a map from macro identifiers to their values.
    StringMap<MacroElement> macroMap;
    StringMap<const DIMacroFile *> macroFileMap;
    std::vector<const DIMacroFile *> macroFileStack;

    for (const DIMacroNode *Node : RawMacros) {
        if (const DIMacroFile *File = dyn_cast<DIMacroFile>(Node))
            macroFileStack.push_back(File);
    }

    while (!macroFileStack.empty()) {
        const DIMacroFile *MF = macroFileStack.back();
        DIMacroNodeArray A = MF->getElements();
        macroFileStack.pop_back();

        for (const DIMacroNode *Node : A)
            if (const DIMacroFile *File = dyn_cast<DIMacroFile>(Node))
                macroFileStack.push_back(File);
            else if (const DIMacro *Macro = dyn_cast<DIMacro>(Node)) {
                std::string macroName = Macro->getName().str();

                // If the macro name contains arguments, remove them
                auto position = macroName.find('(');
                if (position != std::string::npos) {
                    macroName = macroName.substr(0, position);
                }

                MacroElement element;
                element.name = Macro->getName();
                element.body = Macro->getValue();
                element.parentMacro = "N/A";
                element.source = MF;
                element.line = Macro->getLine();
                macroMap[macroName] = element;
            }
    }

    return getAllMacrosOnLine(line, macroMap);
}
